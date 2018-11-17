/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Stanford University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Stephen Ibanez <sibanez@stanford.edu>
 */

#include <bm/bm_sim/parser.h>
#include <bm/bm_sim/tables.h>
#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/event_logger.h>
#include <bm/bm_runtime/bm_runtime.h>
#include <bm/bm_sim/options_parse.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "p4-pipeline.h"

// NOTE: do not include "ns3/log.h" because of name conflict with LOG_DEBUG

#define PACKET_LENGTH_REG_IDX 0

extern int import_primitives();

namespace ns3 {

namespace {

struct hash_ex {
  uint32_t operator()(const char *buf, size_t s) const {
    const uint32_t p = 16777619;
    uint32_t hash = 2166136261;

    for (size_t i = 0; i < s; i++)
      hash = (hash ^ buf[i]) * p;

    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    return static_cast<uint32_t>(hash);
  }
};

struct bmv2_hash {
  uint64_t operator()(const char *buf, size_t s) const {
    return bm::hash::xxh64(buf, s);
  }
};

}  // namespace

// if REGISTER_HASH calls placed in the anonymous namespace, some compiler can
// give an unused variable warning
REGISTER_HASH(hash_ex);
REGISTER_HASH(bmv2_hash);

// initialize static attributes
bm::packet_id_t SimpleP4Pipe::packet_id = 0;
uint8_t SimpleP4Pipe::ns2bm_buf[MAX_PKT_SIZE] = {};

SimpleP4Pipe::SimpleP4Pipe (std::string jsonFile)
{

  add_required_field("standard_metadata", "ingress_port");
  add_required_field("standard_metadata", "packet_length");
  add_required_field("standard_metadata", "instance_type");
  add_required_field("standard_metadata", "egress_spec");
  add_required_field("standard_metadata", "clone_spec");
  add_required_field("standard_metadata", "egress_port");
  // Additional fields
  add_required_field("standard_metadata", "drop");

  force_arith_header("standard_metadata");
  force_arith_header("queueing_metadata");
  force_arith_header("intrinsic_metadata");

  import_primitives();

  // Initialize the switch
  bm::OptionsParser opt_parser;
  opt_parser.config_file_path = jsonFile;
  opt_parser.thrift_port = 9090; // Default thrift port
  opt_parser.debugger_addr = std::string("ipc:///tmp/bmv2-0-debug.ipc");
  opt_parser.notifications_addr = std::string("ipc:///tmp/bmv2-0-notifications.ipc");
  opt_parser.console_logging = true;
//  opt_parser.file_logger = std::string("");

  int status = init_from_options_parser(opt_parser);
  if (status != 0) {
    BMLOG_DEBUG("Failed to initialize the P4 pipeline");
    std::exit(status);
  }

  int thrift_port = get_runtime_port();
  bm_runtime::start_server(this, thrift_port);
  start_and_return();

  // TODO(sibanez): add new constructor arg (std::string commandsFile)
  // Open new process to mimic the following command:
  /*
       subprocess.Popen(['simple_switch_CLI', '--thrift-port', str(thrift_port)],
                        stdin=fin, stdout=fout)
   */
}

void
SimpleP4Pipe::start_and_return_() {

}

int
SimpleP4Pipe::receive_(port_t port_num, const char *buffer, int len)
{
  return 0;
}

Ptr<Packet>
SimpleP4Pipe::process_pipeline(Ptr<Packet> ns3_packet, std_meta_t &std_meta) {
  bm::Parser *parser = this->get_parser("parser");
  bm::Pipeline *mau = this->get_pipeline("ingress");
  bm::Deparser *deparser = this->get_deparser("deparser");
  bm::PHV *phv;

  int len = ns3_packet->GetSize();
  std::cout << "=========== START P4-PIPELINE: pkt_len = " << len << std::endl;
  auto packet = get_bm_packet(ns3_packet);

  BMELOG(packet_in, *packet);

  phv = packet->get_phv();
  phv->reset_metadata();

  /* Set standard metadata */

  // using packet register 0 to store length, this register will be updated for
  // each add_header / remove_header primitive call
  packet->set_register(PACKET_LENGTH_REG_IDX, len);
  phv->get_field("standard_metadata.packet_length").set(std_meta.pkt_len);

  if (phv->has_field("intrinsic_metadata.ingress_global_timestamp"))
    phv->get_field("intrinsic_metadata.ingress_global_timestamp")
          .set(std_meta.timestamp);
  else
    bm::Logger::get()->warn(
          "Your JSON input does not define the ingress_global_timestamp field");

  if (field_exists("queueing_metadata", "enq_qdepth"))
    phv->get_field("queueing_metadata.enq_qdepth").set(std_meta.qdepth);
  else
    bm::Logger::get()->warn(
          "Your JSON input does not define the enq_qdepth field");

  BMLOG_DEBUG_PKT(*packet, "Processing received packet");

  /* Invoke Parser */
  parser->parse(packet.get());

  /* Invoke Match-Action */
  mau->apply(packet.get());

  packet->reset_exit();

  int drop = phv->get_field("standard_metadata.drop").get_int();
  BMLOG_DEBUG_PKT(*packet, "Drop field is {}", drop);

  std_meta.drop = (drop != 0);

  /* Invoke Deparser */
  deparser->deparse(packet.get());

  BMELOG(packet_out, *packet);
  BMLOG_DEBUG_PKT(*packet, "Transmitting packet of size {}",
                   packet->get_data_size());

  return get_ns3_packet(std::move(packet));
}

std::unique_ptr<bm::Packet>
SimpleP4Pipe::get_bm_packet(Ptr<Packet> ns3_packet) {
  port_t port_num = 0; // unused
  int len = ns3_packet->GetSize();

  if (len > MAX_PKT_SIZE)  {
    BMLOG_DEBUG("Packet length {} exceeds MAX_PKT_SIZE", len);
    std::exit(1); // TODO(sibanez): set error code
  }
  ns3_packet->CopyData(ns2bm_buf, len);
  auto bm_packet = new_packet_ptr(port_num, packet_id++, len,
                               bm::PacketBuffer(MAX_PKT_SIZE, (char*)(ns2bm_buf), len));
  return bm_packet;
}

Ptr<Packet>
SimpleP4Pipe::get_ns3_packet(std::unique_ptr<bm::Packet> bm_packet) {
  char *bm_buf = bm_packet.get()->data();
  size_t len = bm_packet.get()->get_data_size();
  std::cout << "=========== END P4-PIPELINE: pkt_len = " << len << std::endl;
  return Create<Packet> ((uint8_t*)(bm_buf), len);
}

}


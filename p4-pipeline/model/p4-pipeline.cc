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

#include <bm/bm_sim/queue.h>
#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/parser.h>
#include <bm/bm_sim/tables.h>
#include <bm/bm_sim/switch.h>
#include <bm/bm_sim/event_logger.h>
#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/simple_pre.h>

#include <bm/bm_runtime/bm_runtime.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <thread>
#include <fstream>
#include <string>
#include <chrono>

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/packet.h"
#include "p4-pipeline.h"

namespace ns3 {

SimpleP4Pipe::SimpleP4Pipe (std::string jsonFile)
  : input_buffer(1024), output_buffer(128)
{
  add_required_field("standard_metadata", "egress_port");
  add_required_field("standard_metadata", "egress_qdepth");
  add_required_field("standard_metadata", "drop");

  force_arith_header("standard_metadata");

  // simulate command: sudo ./simple_p4_pipe <path to JSON file>
  int argc = 2;
  char* argv[2] = {"./simple_p4_pipe", jsonFile};
  int status = init_from_command_line_options(argc, argv);
  if (status != 0)
    NS_LOG_ERROR("Failed to initialize the P4 pipeline");

  int thrift_port = get_runtime_port();
  bm_runtime::start_server(this, thrift_port);
  start_and_return();
}

void
SimpleP4Pipe::start_and_return_() override {

}

int
SimpleP4Pipe::receive_(port_t port_num, const char *buffer, int len) override
{
  return 0;
}

Ptr<Packet>
SimpleP4Pipe::process_pipeline(Ptr<Packet> packet, std_meta_t &std_meta) {
  bm::Parser *parser = this->get_parser("parser");
  bm::Pipeline *mau = this->get_pipeline("pipe");
  bm::Deparser *deparser = this->get_deparser("deparser");
  bm::PHV *phv;

  std::unique_ptr<bm::Packet> get_bm_packet(packet);

  phv = packet->get_phv();

  BMLOG_DEBUG_PKT(*packet, "Processing packet for port {}",
                  egress_port);

  phv->get_field("standard_metadata.egress_port").set(std_meta.egress_port);
  phv->get_field("standard_metadata.egress_qdepth").set(std_meta.egress_qdepth);

  parser->parse(packet.get());
  mau->apply(packet.get());

  int drop = phv->get_field("standard_metadata.drop").get_int();
  BMLOG_DEBUG_PKT(*packet, "Drop bit is {}", drop);

  std_meta.drop = (drop != 0);

  deparser->deparse(packet.get());

  return get_ns3_packet(packet);
}

std::unique_ptr<bm::Packet>
SimpleP4Pipe::get_bm_packet(Ptr<Packet> ns3_packet) {
  static int pkt_id = 0;

  port_t port_num = 0; // unused
  int len = ns3_packet->GetSize();

  uint8_t* tmp_buf = new uint8_t[len];
  ns3_packet->CopyData(tmp_buf, len);
  auto bm_packet = new_packet_ptr(port_num, pkt_id++, len,
                               bm::PacketBuffer(2048, std::static_cast<char*>(tmp_buf), len));
  delete tmp_buf;
  return bm_packet;
}

Ptr<Packet>
SimpleP4Pipe::get_ns3_packet(std::unique_ptr<bm:Packet> bm_packet) {
  char *bm_buf = bm_packet.get()->data();
  size_t len = bm_packet.get()->get_data_size();
  return Create<Packet> (std::static_cast<uint8_t*>(bm_buf), len);
}

}


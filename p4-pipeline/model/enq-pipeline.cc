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
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include "enq-pipeline.h"

// NOTE: do not include "ns3/log.h" because of name conflict with LOG_DEBUG

extern int import_primitives();

namespace ns3 {

EnqP4Pipe::EnqP4Pipe (std::string jsonFile)
{
  // Required fields
  add_required_field("standard_metadata", "pkt_len");
  add_required_field("standard_metadata", "flow_hash");
  add_required_field("standard_metadata", "buffer_id");
  add_required_field("standard_metadata", "partition_id");
  add_required_field("standard_metadata", "partition_size");
  add_required_field("standard_metadata", "partition_max_size");
  add_required_field("standard_metadata", "timestamp");
  add_required_field("standard_metadata", "is_leaf");
  add_required_field("standard_metadata", "child_node_id");
  add_required_field("standard_metadata", "child_pifo_id");
  // P4 program outputs
  add_required_field("standard_metadata", "rank");
  add_required_field("standard_metadata", "pifo_id");
  add_required_field("standard_metadata", "enq_delay");
  // P4 program tracedata
  add_required_field("standard_metadata", "trace_var1");
  add_required_field("standard_metadata", "trace_var2");
  add_required_field("standard_metadata", "trace_var3");
  add_required_field("standard_metadata", "trace_var4");

  force_arith_header("standard_metadata");

  import_primitives();

  // Initialize the switch
  bm::OptionsParser opt_parser;
  opt_parser.config_file_path = jsonFile;
  opt_parser.debugger_addr = std::string("ipc:///tmp/bmv2-") +
                             std::to_string(thrift_port) +
                             std::string("-debug.ipc");
  opt_parser.notifications_addr = std::string("ipc:///tmp/bmv2-") +
                             std::to_string(thrift_port) +
                             std::string("-notifications.ipc");
  opt_parser.file_logger = std::string("/tmp/bmv2-") +
                             std::to_string(thrift_port) +
                             std::string("-pipeline.log");
  opt_parser.thrift_port = thrift_port++;

  int status = init_from_options_parser(opt_parser);
  if (status != 0) {
    BMLOG_DEBUG("Failed to initialize the P4 pipeline");
    std::exit(status);
  }

}

void
EnqP4Pipe::process_pipeline(std_enq_meta_t &std_meta) {
  bm::Pipeline *mau = this->get_pipeline("ingress");
  bm::PHV *phv;

  // TODO(sibanez): is this set correctly?
  auto packet = new_packet_ptr(0, packet_id++, 0, bm::PacketBuffer (0));

  BMELOG(packet_in, *packet);

  phv = packet->get_phv();
  phv->reset_metadata();

  /* Set standard metadata */

  // using packet register 0 to store length, this register will be updated for
  // each add_header / remove_header primitive call
  packet->set_register(PACKET_LENGTH_REG_IDX, 0);
  phv->get_field("standard_metadata.pkt_len").set(std_meta.sched_meta.pkt_len);
  phv->get_field("standard_metadata.flow_hash").set(std_meta.sched_meta.flow_hash);
  phv->get_field("standard_metadata.buffer_id").set(std_meta.sched_meta.buffer_id);
  phv->get_field("standard_metadata.partition_id").set(std_meta.sched_meta.partition_id);
  phv->get_field("standard_metadata.partition_size").set(std_meta.sched_meta.partition_size);
  phv->get_field("standard_metadata.partition_max_size").set(std_meta.sched_meta.partition_max_size);
  phv->get_field("standard_metadata.timestamp").set(std_meta.timestamp);
  phv->get_field("standard_metadata.is_leaf").set(std_meta.is_leaf);
  phv->get_field("standard_metadata.child_node_id").set(std_meta.child_node_id);
  phv->get_field("standard_metadata.child_pifo_id").set(std_meta.child_pifo_id);

  phv->get_field("standard_metadata.trace_var1").set(std_meta.trace_var1);
  phv->get_field("standard_metadata.trace_var2").set(std_meta.trace_var2);
  phv->get_field("standard_metadata.trace_var3").set(std_meta.trace_var3);
  phv->get_field("standard_metadata.trace_var4").set(std_meta.trace_var4);

  BMLOG_DEBUG_PKT(*packet, "Processing received packet");

  /* Invoke Match-Action */
  mau->apply(packet.get());

  packet->reset_exit();

  /* Set trace variables */
  std_meta.trace_var1 = phv->get_field("standard_metadata.trace_var1").get_int();
  std_meta.trace_var2 = phv->get_field("standard_metadata.trace_var2").get_int();
  std_meta.trace_var3 = phv->get_field("standard_metadata.trace_var3").get_int();
  std_meta.trace_var4 = phv->get_field("standard_metadata.trace_var4").get_int();

  /* Set output fields */
  int rank = phv->get_field("standard_metadata.rank").get_int();
  BMLOG_DEBUG_PKT(*packet, "rank field is {}", rank);
  std_meta.rank = rank;

  int pifo_id = phv->get_field("standard_metadata.pifo_id").get_int();
  BMLOG_DEBUG_PKT(*packet, "pifo_id field is {}", pifo_id);
  std_meta.pifo_id = pifo_id;

  int enq_delay = phv->get_field("standard_metadata.enq_delay").get_int();
  BMLOG_DEBUG_PKT(*packet, "enq_delay field is {}", enq_delay);
  std_meta.enq_delay = enq_delay;

  uint64_t tx_time = phv->get_field("standard_metadata.tx_time").get_uint64();
  BMLOG_DEBUG_PKT(*packet, "tx_time field is {}", tx_time);
  std_meta.tx_time = (int64_t)tx_time; //TODO(sibanez): can we get rid of this cast?

  int tx_delta = phv->get_field("standard_metadata.tx_delta").get_int();
  BMLOG_DEBUG_PKT(*packet, "tx_delta field is {}", tx_delta);
  std_meta.tx_delta = tx_delta;

  BMELOG(packet_out, *packet);
  BMLOG_DEBUG_PKT(*packet, "Transmitting packet");

}

}


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

#include "classification-pipeline.h"

// NOTE: do not include "ns3/log.h" because of name conflict with LOG_DEBUG

namespace ns3 {

ClassificationP4Pipe::ClassificationP4Pipe (std::string jsonFile)
{
  // Required fields
  add_required_field("standard_metadata", "pkt_len");
  add_required_field("standard_metadata", "flow_hash");
  add_required_field("standard_metadata", "timestamp");
  // P4 program outputs
  add_required_field("standard_metadata", "buffer_id");
  add_required_field("standard_metadata", "leaf_id");
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
ClassificationP4Pipe::process_pipeline(std_class_meta_t &std_meta) {
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
  packet->set_register(PACKET_LENGTH_REG_IDX, len);
  phv->get_field("standard_metadata.pkt_len").set(std_meta.pkt_len);
  phv->get_field("standard_metadata.flow_hash").set(std_meta.flow_hash);
  phv->get_field("standard_metadata.timestamp").set(std_meta.timestamp);
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
  int buffer_id = phv->get_field("standard_metadata.buffer_id").get_int();
  BMLOG_DEBUG_PKT(*packet, "buffer_id field is {}", buffer_id);
  std_meta.buffer_id = buffer_id;

  int leaf_id = phv->get_field("standard_metadata.leaf_id").get_int();
  BMLOG_DEBUG_PKT(*packet, "leaf_id field is {}", leaf_id);
  std_meta.leaf_id = leaf_id;

  BMELOG(packet_out, *packet);
  BMLOG_DEBUG_PKT(*packet, "Transmitting packet");

}

}


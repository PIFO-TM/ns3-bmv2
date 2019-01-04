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

#include "deq-pipeline.h"

// NOTE: do not include "ns3/log.h" because of name conflict with LOG_DEBUG

namespace ns3 {

DeqP4Pipe::DeqP4Pipe (std::string jsonFile)
{
  // Required fields
  add_required_field("standard_metadata", "timestamp");
  add_required_field("standard_metadata", "is_leaf");
  // pifo0 metadata
  add_required_field("standard_metadata", "pifo0_is_empty");
  add_required_field("standard_metadata", "pifo0_last_deq_time");
  add_required_field("standard_metadata", "pifo0_child_node_id");
  add_required_field("standard_metadata", "pifo0_child_pifo_id");
  add_required_field("standard_metadata", "pifo0_rank");
  add_required_field("standard_metadata", "pifo0_tx_time");
  add_required_field("standard_metadata", "pifo0_tx_delta");
  add_required_field("standard_metadata", "pifo0_pkt_len");
  // pifo1 metadata
  add_required_field("standard_metadata", "pifo1_is_empty");
  add_required_field("standard_metadata", "pifo1_last_deq_time");
  add_required_field("standard_metadata", "pifo1_child_node_id");
  add_required_field("standard_metadata", "pifo1_child_pifo_id");
  add_required_field("standard_metadata", "pifo1_rank");
  add_required_field("standard_metadata", "pifo1_tx_time");
  add_required_field("standard_metadata", "pifo1_tx_delta");
  add_required_field("standard_metadata", "pifo1_pkt_len");
  // pifo2 metadata
  add_required_field("standard_metadata", "pifo2_is_empty");
  add_required_field("standard_metadata", "pifo2_last_deq_time");
  add_required_field("standard_metadata", "pifo2_child_node_id");
  add_required_field("standard_metadata", "pifo2_child_pifo_id");
  add_required_field("standard_metadata", "pifo2_rank");
  add_required_field("standard_metadata", "pifo2_tx_time");
  add_required_field("standard_metadata", "pifo2_tx_delta");
  add_required_field("standard_metadata", "pifo2_pkt_len");
  // P4 program outputs
  add_required_field("standard_metadata", "pifo_id");
  add_required_field("standard_metadata", "deq_delay");
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
DeqP4Pipe::process_pipeline(std_deq_meta_t &std_meta) {
  bm::Pipeline *mau = this->get_pipeline("ingress");
  bm::PHV *phv;

  // TODO(sibanez): set this correctly
  auto packet = new_packet_ptr();

  BMELOG(packet_in, *packet);

  phv = packet->get_phv();
  phv->reset_metadata();

  /* Set standard metadata */

  // using packet register 0 to store length, this register will be updated for
  // each add_header / remove_header primitive call
  packet->set_register(PACKET_LENGTH_REG_IDX, len);
  phv->get_field("standard_metadata.timestamp").set(std_meta.timestamp);
  phv->get_field("standard_metadata.is_leaf").set(std_meta.is_leaf);
  // pifo0 metadata
  phv->get_field("standard_metadata.pifo0_is_empty").set(std_meta.pifo_is_empty[0]);
  phv->get_field("standard_metadata.pifo0_last_deq_time").set(std_meta.pifo_last_deq_time[0]);
  phv->get_field("standard_metadata.pifo0_child_node_id").set(std_meta.pifo_child_node_id[0]);
  phv->get_field("standard_metadata.pifo0_child_pifo_id").set(std_meta.pifo_child_pifo_id[0]);
  phv->get_field("standard_metadata.pifo0_rank").set(std_meta.pifo_rank[0]);
  phv->get_field("standard_metadata.pifo0_tx_time").set(std_meta.pifo_tx_time[0]);
  phv->get_field("standard_metadata.pifo0_tx_delta").set(std_meta.pifo_tx_delta[0]);
  phv->get_field("standard_metadata.pifo0_pkt_len").set(std_meta.pifo_pkt_len[0]);
  // pifo1 metadata
  phv->get_field("standard_metadata.pifo1_is_empty").set(std_meta.pifo_is_empty[1]);
  phv->get_field("standard_metadata.pifo1_last_deq_time").set(std_meta.pifo_last_deq_time[1]);
  phv->get_field("standard_metadata.pifo1_child_node_id").set(std_meta.pifo_child_node_id[1]);
  phv->get_field("standard_metadata.pifo1_child_pifo_id").set(std_meta.pifo_child_pifo_id[1]);
  phv->get_field("standard_metadata.pifo1_rank").set(std_meta.pifo_rank[1]);
  phv->get_field("standard_metadata.pifo1_tx_time").set(std_meta.pifo_tx_time[1]);
  phv->get_field("standard_metadata.pifo1_tx_delta").set(std_meta.pifo_tx_delta[1]);
  phv->get_field("standard_metadata.pifo1_pkt_len").set(std_meta.pifo_pkt_len[1]);
  // pifo2 metadata
  phv->get_field("standard_metadata.pifo2_is_empty").set(std_meta.pifo_is_empty[2]);
  phv->get_field("standard_metadata.pifo2_last_deq_time").set(std_meta.pifo_last_deq_time[2]);
  phv->get_field("standard_metadata.pifo2_child_node_id").set(std_meta.pifo_child_node_id[2]);
  phv->get_field("standard_metadata.pifo2_child_pifo_id").set(std_meta.pifo_child_pifo_id[2]);
  phv->get_field("standard_metadata.pifo2_rank").set(std_meta.pifo_rank[2]);
  phv->get_field("standard_metadata.pifo2_tx_time").set(std_meta.pifo_tx_time[2]);
  phv->get_field("standard_metadata.pifo2_tx_delta").set(std_meta.pifo_tx_delta[2]);
  phv->get_field("standard_metadata.pifo2_pkt_len").set(std_meta.pifo_pkt_len[2]);
  // initialize tracedata
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
  int pifo_id = phv->get_field("standard_metadata.pifo_id").get_int();
  BMLOG_DEBUG_PKT(*packet, "pifo_id field is {}", pifo_id);
  std_meta.pifo_id = pifo_id;

  int deq_delay = phv->get_field("standard_metadata.deq_delay").get_int();
  BMLOG_DEBUG_PKT(*packet, "deq_delay field is {}", deq_delay);
  std_meta.deq_delay = deq_delay;

  BMELOG(packet_out, *packet);
  BMLOG_DEBUG_PKT(*packet, "Transmitting packet");

}

}


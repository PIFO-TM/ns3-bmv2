/* Copyright 2018-present Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 * Stephen Ibanez (sibanez@stanford.edu)
 *
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

using bm::Switch;
using bm::Queue;
using bm::Packet;
using bm::PHV;
using bm::Parser;
using bm::Deparser;
using bm::Pipeline;

SimplePipe::SimplePipe ()
  : input_buffer(1024), output_buffer(128)
{
  add_required_field("standard_metadata", "egress_port");
  add_required_field("standard_metadata", "egress_qdepth");
  add_required_field("standard_metadata", "drop");

  force_arith_header("standard_metadata");
}

void
start_and_return_() override {

}

int
SimplePipe::receive_(port_t port_num, const char *buffer, int len) override
{
  return 0;
}

void SimplePipe::process_pipeline(std::unique_ptr<Packet> packet, std_meta_t &std_meta) {
  Parser *parser = this->get_parser("parser");
  Pipeline *mau = this->get_pipeline("pipe");
  Deparser *deparser = this->get_deparser("deparser");
  PHV *phv;

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
}


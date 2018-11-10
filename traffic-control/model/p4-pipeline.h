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

typedef struct {
  int egress_port;
  int egress_qdepth;
  bool drop;
} std_meta_t;

class SimplePipe : public Switch {
 public:
  /**
   * \brief SimplePipe constructor
   */
  SimplePipe();

  /**
   * \brief Unused
   */
  int receive_(port_t port_num, const char *buffer, int len) override; 

  /**
   * \brief Unused
   */
  void start_and_return_() override; 

  /**
   * \brief Invoke the P4 processing pipeline (parser, match-action, deparser)
   */
  void process_pipeline(std::unique_ptr<Packet> packet, std_meta_t &std_meta);
};


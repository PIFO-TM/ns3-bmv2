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

#include "base-p4-pipe.h"

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
bm::packet_id_t BaseP4Pipe::packet_id = 0;
int BaseP4Pipe::thrift_port = 9090;

void
BaseP4Pipe::run_cli(std::string commandsFile) {
  int port = get_runtime_port();
  bm_runtime::start_server(this, port);
  start_and_return();

  std::this_thread::sleep_for(std::chrono::seconds(5));

  // Run the CLI commands to populate table entries
  std::string cmd = "run_bmv2_CLI --thrift_port " + std::to_string(port) + " " + commandsFile;
  std::system (cmd.c_str());
}

void
BaseP4Pipe::start_and_return_() {

}

int
BaseP4Pipe::receive_(port_t port_num, const char *buffer, int len)
{
  return 0;
}

}


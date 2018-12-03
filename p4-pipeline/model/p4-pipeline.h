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
 * Author: Stephen Ibanez <sibanez@stanford.edu>
 */

#ifndef P4_PIPELINE_H
#define P4_PIPELINE_H

#define MAX_PKT_SIZE 2048

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>

#include <memory>
#include <string>

#include "ns3/pointer.h"
#include "ns3/packet.h"

namespace ns3 {

/**
 * \brief The standard metadata for the P4 pipeline
 */
typedef struct {
  int qdepth;          // input
  int avg_qdepth;      // input
  int64_t timestamp;   // input
  int64_t idle_time;   // input
  int64_t qlatency;    // input
  uint32_t pkt_len;    // input
  uint16_t l3_proto;   // input
  uint32_t flow_hash;  // input
  bool drop;           // output
  bool mark;           // output
  uint32_t trace_var1; // input/output
  uint32_t trace_var2; // input/output
  uint32_t trace_var3; // input/output
  uint32_t trace_var4; // input/output
} std_meta_t;

/**
 * \ingroup p4-pipeline
 *
 * A P4 programmable pipeline.
 */
class SimpleP4Pipe : public bm::Switch {
 public:
  /**
   * \brief SimplePipe constructor
   */
  SimpleP4Pipe (std::string jsonFile);

  /**
   * \brief Run the provided CLI commands to populate table entries
   */
  void run_cli(std::string commandsFile);

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
  Ptr<Packet> process_pipeline(Ptr<Packet> ns3_packet, std_meta_t &std_meta);

 private:
  /**
   * \brief Convert the NS3 packet ptr into a bmv2 pkt ptr
   */
  std::unique_ptr<bm::Packet> get_bm_packet(Ptr<Packet> ns3_packet);

  /**
   * \brief Convert the NS3 packet ptr into a bmv2 pkt ptr
   */
  Ptr<Packet> get_ns3_packet(std::unique_ptr<bm::Packet> bm_packet);

 private:
  static int thrift_port;
  static bm::packet_id_t packet_id;
  static uint8_t ns2bm_buf[MAX_PKT_SIZE];
};

}

#endif /* P4_PIPELINE_H */


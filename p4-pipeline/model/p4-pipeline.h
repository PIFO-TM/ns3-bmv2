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

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>

#include <memory>
#include <string>

#include "ns3/pointer.h"
#include "ns3/packet.h"

#include "base-p4-pipe.h"

namespace ns3 {

/**
 * \brief The standard metadata for the P4 pipeline
 */
typedef struct {
  uint32_t qdepth;
  uint32_t qdepth_bytes;
  uint32_t avg_qdepth;
  uint32_t avg_qdepth_bytes;
  int64_t timestamp; 
  int64_t idle_time; 
  int64_t qlatency; 
  uint32_t avg_deq_rate_bytes;
  uint32_t pkt_len;
  uint32_t pkt_len_bytes;
  uint16_t l3_proto;
  uint32_t flow_hash;
  bool ingress_trigger;
  bool timer_trigger;
  // drop trigger metadata
  bool     drop_trigger;
  int64_t  drop_timestamp;
  uint32_t drop_qdepth;
  uint32_t drop_qdepth_bytes;
  uint32_t drop_avg_qdepth;
  uint32_t drop_avg_qdepth_bytes;
  uint32_t drop_pkt_len;
  uint32_t drop_pkt_len_bytes;
  uint16_t drop_l3_proto;
  uint32_t drop_flow_hash;
  // enqueue trigger metadata
  bool     enq_trigger;
  int64_t  enq_timestamp;
  uint32_t enq_qdepth;
  uint32_t enq_qdepth_bytes;
  uint32_t enq_avg_qdepth;
  uint32_t enq_avg_qdepth_bytes;
  uint32_t enq_pkt_len;
  uint32_t enq_pkt_len_bytes;
  uint16_t enq_l3_proto;
  uint32_t enq_flow_hash;
  // dequeue trigger metadata
  bool     deq_trigger;
  int64_t  deq_enq_timestamp;
  uint32_t deq_qdepth;
  uint32_t deq_qdepth_bytes;
  uint32_t deq_avg_qdepth;
  uint32_t deq_avg_qdepth_bytes;
  int64_t  deq_timestamp;
  uint32_t deq_pkt_len;
  uint32_t deq_pkt_len_bytes;
  uint16_t deq_l3_proto;
  uint32_t deq_flow_hash;
  // P4 program outputs
  bool drop;
  bool mark;
  // P4 program tracedata
  uint32_t trace_var1;          // input/output
  uint32_t trace_var2;          // input/output
  uint32_t trace_var3;          // input/output
  uint32_t trace_var4;          // input/output
} std_meta_t;

/**
 * \ingroup p4-pipeline
 *
 * A P4 programmable pipeline.
 */
class SimpleP4Pipe : public BaseP4Pipe {
 public:
  /**
   * \brief SimpleP4Pipe constructor
   */
  SimpleP4Pipe (std::string jsonFile);

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
  static bm::packet_id_t packet_id;
  static uint8_t ns2bm_buf[MAX_PKT_SIZE];
};

}

#endif /* P4_PIPELINE_H */


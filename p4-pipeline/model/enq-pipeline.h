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

#ifndef ENQ_PIPELINE_H
#define ENQ_PIPELINE_H

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>

#include <memory>
#include <string>

#include "ns3/pointer.h"
#include "ns3/packet.h"

#include "base-p4-pipe.h"

namespace ns3 {

/**
 * \brief Scheduling metadata fields
 */
typedef struct {
  uint32_t pkt_len;
  uint32_t flow_hash;
  uint32_t buffer_id;
  uint32_t partition_id;
  uint32_t partition_size;
  uint32_t partition_max_size;
} sched_meta_t;

/**
 * \brief The standard metadata for the enqueue pipeline
 */
typedef struct {
  // scheduling/shaping metadata
  bool enq_trigger;
  sched_meta_t sched_meta;
  int64_t timestamp;
  bool is_leaf;
  uint8_t child_node_id;
  uint8_t child_pifo_id;
  // dequeue event metadata
  bool deq_trigger;
  uint8_t deq_node_id;
  uint8_t deq_pifo_id;
  uint32_t deq_rank;
  int64_t deq_tx_time;
  uint32_t deq_tx_delta;
  uint32_t deq_user_meta;
  sched_meta_t deq_sched_meta;
  // P4 program outputs
  uint32_t rank;
  uint8_t pifo_id;
  uint32_t enq_delay; // non-zero value will reschedule enqueue operation to complete in the future
  int64_t tx_time;    // stored in PIFO entry
  uint32_t tx_delta;  // stored in PIFO entry
  uint32_t user_meta; // stored in PIFO entry
  // P4 program tracedata
  uint32_t trace_var1;          // input/output
  uint32_t trace_var2;          // input/output
  uint32_t trace_var3;          // input/output
  uint32_t trace_var4;          // input/output
} std_enq_meta_t;

/**
 * \ingroup p4-pipeline
 *
 * Architecture used to implement programmable enqueue logic in PIFO trees.
 */
class EnqP4Pipe : public BaseP4Pipe {
 public:
  /**
   * \brief SimplePipe constructor
   */
  EnqP4Pipe (std::string jsonFile);

  /**
   * \brief Invoke the P4 processing pipeline (match-action only)
   */
  void process_pipeline(std_enq_meta_t &std_meta);
};

}

#endif /* ENQ_PIPELINE_H */


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

#ifndef DEQ_PIPELINE_H
#define DEQ_PIPELINE_H

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>

#include <memory>
#include <string>

#include "ns3/pointer.h"
#include "ns3/packet.h"

#include "base-p4-pipe.h"

#define MAX_NUM_PIFOS 3

namespace ns3 {

/**
 * \brief The standard metadata for the dequeue pipeline
 */
typedef struct {
  // scheduling/shaping metadata
  int64_t timestamp;
  bool is_leaf;
  // pifo metadata
  bool     pifo_is_empty[MAX_NUM_PIFOS];
  int64_t  pifo_last_deq_time[MAX_NUM_PIFOS];
  uint8_t  pifo_child_node_id[MAX_NUM_PIFOS];
  uint8_t  pifo_child_pifo_id[MAX_NUM_PIFOS];
  uint32_t pifo_rank[MAX_NUM_PIFOS];
  int64_t  pifo_tx_time[MAX_NUM_PIFOS];
  uint32_t pifo_tx_delta[MAX_NUM_PIFOS];
  uint32_t pifo_pkt_len[MAX_NUM_PIFOS];

  // P4 program outputs
  uint8_t pifo_id;
  uint32_t deq_delay; // non-zero value will schedule dequeue operation to complete in the future
  // P4 program tracedata
  uint32_t trace_var1;          // input/output
  uint32_t trace_var2;          // input/output
  uint32_t trace_var3;          // input/output
  uint32_t trace_var4;          // input/output
} std_deq_meta_t;

/**
 * \ingroup p4-pipeline
 *
 * Architecture used to implement P4 programmable dequeue logic for PIFO tree
 */
class DeqP4Pipe : public BaseP4Pipe {
 public:
  /**
   * \brief DeqP4Pipe constructor
   */
  DeqP4Pipe (std::string jsonFile);

  /**
   * \brief Invoke the P4 processing pipeline (match-action only)
   */
  void process_pipeline(std_deq_meta_t &std_meta);
};

}

#endif /* DEQ_PIPELINE_H */


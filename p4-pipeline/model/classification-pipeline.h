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

#ifndef CLASSIFICATION_PIPELINE_H
#define CLASSIFICATION_PIPELINE_H

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>

#include <memory>
#include <string>

#include "ns3/pointer.h"
#include "ns3/packet.h"

#include "base-p4-pipe.h"

namespace ns3 {

/**
 * \brief The standard metadata for PifoTreeQueueDisc classification pipeline
 */
typedef struct {
  // Input metadata
  uint32_t pkt_len;
  uint32_t flow_hash;
  int64_t timestamp;
  // Output metadata
  uint32_t buffer_id;
  uint32_t leaf_id;
  // P4 program tracedata
  uint32_t trace_var1;          // input/output
  uint32_t trace_var2;          // input/output
  uint32_t trace_var3;          // input/output
  uint32_t trace_var4;          // input/output
} std_class_meta_t;

/**
 * \ingroup p4-pipeline
 *
 * Architecture used to implement P4 classification logic for PifoTreeQueueDisc
 */
class ClassificationP4Pipe : public BaseP4Pipe {
 public:
  /**
   * \brief ClassificationP4Pipe constructor
   */
  ClassificationP4Pipe (std::string jsonFile);

  /**
   * \brief Invoke the P4 processing pipeline (match-action only)
   */
  void process_pipeline(std_class_meta_t &std_meta);
};

}

#endif /* CLASSIFICATION_PIPELINE_H */


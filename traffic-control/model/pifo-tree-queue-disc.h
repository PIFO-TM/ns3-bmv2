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
 * Author:  Stephen Ibanez <sibanez@stanford.edu>
 */

#ifndef PIFO_TREE_QDISC_H
#define PIFO_TREE_QDISC_H

#include "ns3/queue-disc.h"
#include "ns3/classification-pipeline.h"
#include "ns3/enq-pipeline.h"
#include "ns3/deq-pipeline.h"
#include "json/json.h"
#include "pifo-tree-buffer.h"
#include "pifo-tree-node.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * TODO (sibanez): detailed description ...
 * Scheduling is performed using a tree of PIFO queue discs. The is configured
 * using a JSON file.
 */
class PifoTreeQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief PifoTreeQueueDisc constructor
   *
   */
  PifoTreeQueueDisc ();

  virtual ~PifoTreeQueueDisc();

  // Reasons for dropping packets
  // TODO(sibanez): list drop reasons
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Buffer limit exceeded";

private:
  ClassificationP4Pipe* m_classPipe;

  /// JSON file which speicifes the configuration of the Pifo Tree
  std::string m_pifoTreeJson;
  /// Store pointers to all PifoTreeNodes
  std::vector<Ptr<PifoTreeNode>> m_nodes;
  /// The packet buffer used to make drop decisions
  PifoTreeBuffer m_buffer;

  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  // TODO(sibanez): Is this method needed? Use default base class DoPeek() implementation for now
  //virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  /**
   * \brief Helper method for DoDequeue()
   * \param deqData specifies which node and PIFO to dequeue from
   */
  virtual Ptr<QueueDiscItem> DoDequeue (Ptr<DequeueData> deqData) override;

  /**
   * \brief Configure classification logic
   */
  void ConfigClassification (Json::Value classLogic);

  /**
   * \brief Configure buffer sizes
   */
  void ConfigBuffers (Json::Value bufferSizes);

  /**
   * \brief Configure a PIFO tree node
   */
  void ConfigNode (Json::Value jsonRoot, std::string param);

  /**
   * \brief Build and configure the PIFO tree queue disc from the provided JSON file
   */
  void BuildPifoTree (std::string pifoTreeJson);

  /**
   * \brief Attempt to enqueue the queue disc item into the specified buffer
   * \param bufID ID of the buffer to enqueue into
   * \param item ptr to the queue disc item being enqueued
   * \returns bool indicating whether or not enqueue operation was successful
   */
   bool EnqueueBuffer (uint32_t bufID, Ptr<QueueDiscItem> item);

  /**
   * \brief Enqueue into the specified leaf node.
   * \param leafID global ID of the desired leaf node to enqueue into
   * \param item ptr to the queue disc item being enqueued
   * \returns bool indicating whether or not enqueue operation was successful
   */
   bool EnqueueLeaf (uint32_t leafID, Ptr<QueueDiscItem> item);

};

} // namespace ns3

#endif /* PIFO_TREE_QDISC_H */

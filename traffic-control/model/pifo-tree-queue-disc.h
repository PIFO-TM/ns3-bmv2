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
#include "ns3/json.h"
#include "pifo-tree-buffer.h"
#include "pifo-tree-node.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * TODO (sibanez): detailed description ...
 * Scheduling is performed using a tree of PIFO nodes. The queue disc is configured
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
   */
  PifoTreeQueueDisc ();

  virtual ~PifoTreeQueueDisc();

  /**
   * \brief Return a pointer to the buffer attached to this PifoTreeQueueDisc
   */
  Ptr<PifoTreeBuffer> GetBuffer (void);

  // Reasons for dropping packets
  // TODO(sibanez): list drop reasons
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Buffer limit exceeded";
  static constexpr const char* PIFO_TREE_DROP = "Failed to enqueue into PifoTree (should not happen)";

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  // TODO(sibanez): Is this method needed? Use default base class DoPeek() implementation for now
  //virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  /// Get the JSON source file
  std::string GetJsonFile (void) const;

  /// Set the JSON source file
  void SetJsonFile (std::string jsonFile);

  /**
   * \brief A default implementation of this function is provided in the QueueDisc class.
   *  Here we override it to extract the nodeID and pifoID stored in the deqData object.
   */
  virtual Ptr<QueueDiscItem> DoDequeue (Ptr<QueueDiscDeqData> deqData) override;

  /**
   * \brief This is the DoDequeue() method that does all the work.
   * \param nodeID the global ID of the node to start dequeuing from
   * \param pifoID the ID of the PIFO within the node to dequeue from
   */
  Ptr<QueueDiscItem> DoDequeue (uint32_t nodeID, uint8_t pifoID);

  /**
   * \brief Configure classification logic
   */
  void ConfigClassification (Json::Value& classLogic);

  /**
   * \brief Configure each PIFO tree node
   */
  void ConfigNodes (Json::Value& jsonRoot, std::string param);

  /**
   * \brief Build and configure the PIFO tree queue disc from the provided JSON file
   */
  void BuildPifoTree (std::string pifoTreeJson);

  /**
   * \brief Enqueue into the specified leaf node.
   * \param leafID global ID of the desired leaf node to enqueue into
   * \param item ptr to the queue disc item being enqueued
   * \param sched_meta the scheduling metadata associated with this packet
   * \returns bool indicating whether or not enqueue operation was successful
   */
   bool EnqueueLeaf (uint32_t leafID, Ptr<QueueDiscItem> item, sched_meta_t sched_meta);

  //
  // PifoTreeQueueDisc Attributes
  //

  ClassificationP4Pipe* m_classPipe;

  /// JSON file which speicifes the configuration of the Pifo Tree
  std::string m_pifoTreeJson;
  std::string m_jsonDir;
  /// Store pointers to all PifoTreeNodes
  std::vector<Ptr<PifoTreeNode>> m_nodes;
  /// The packet buffer used to make drop decisions
  Ptr<PifoTreeBuffer> m_buffer;

  TracedValue<uint32_t> m_p4ClassVar1; //!< 1st traced P4 classification variable
  TracedValue<uint32_t> m_p4ClassVar2; //!< 2nd traced P4 classification variable
  TracedValue<uint32_t> m_p4ClassVar3; //!< 3rd traced P4 classification variable
  TracedValue<uint32_t> m_p4ClassVar4; //!< 4th traced P4 classification variable

  /// Traced callback: fired when a packet is enqueued into a partition
  TracedCallback<Ptr<const QueueDiscItem>, uint32_t > m_traceBufferEnqueue;
  /// Traced callback: fired when a packet is dequeued from a partition
  TracedCallback<Ptr<const QueueDiscItem>, uint32_t > m_traceBufferDequeue;

};

} // namespace ns3

#endif /* PIFO_TREE_QDISC_H */

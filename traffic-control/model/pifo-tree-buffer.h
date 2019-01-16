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
 * Author:  Stpehen Ibanez <sibanez@stanford.edu>
 */

#ifndef PIFO_TREE_BUFFER_H
#define PIFO_TREE_BUFFER_H

#include "ns3/object.h"
#include "ns3/traced-value.h"
#include "ns3/traced-callback.h"
#include "ns3/queue-item.h"
#include "json/json.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * TODO (sibanez): detailed description ...
 * The packet buffer for the PifoTreeQueueDisc. Decides if there is enough buffer space
 * for the incoming packet.
 */
class PifoTreeBuffer : public Object {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief PifoTreeQueueDisc constructor
   */
  PifoTreeBuffer ();

  virtual ~PifoTreeBuffer();

  /**
   * \brief Check if packet will fit into buffer
   * \param configRoot root of the buffer configuration portion of the JSON file
   * \return bool indicating whether or not configuration was successful
   */
  bool Configure (Json::Value configRoot);

  /**
   * \brief Check if packet will fit into buffer
   * \param bufID the ID to use to check for buffer space
   * \param item the packet to enqueue
   * \param sched_meta struct reference, buffering related metadata fields are populated
   * \return bool indicating enqueue operation was successful
   */
  bool Enqueue (uint32_t bufID, Ptr<QueueDiscItem> item, sched_meta_t& sched_meta);

  /**
   * \brief Remove packet from specified buffer index
   * \param bufIndex the buffer index into which the packet was originally enqueued
   * \param item the packet to dequeue
   * \return bool indicating whether or not dequeue was successful
   */
  bool Dequeue (uint32_t partitionID, Ptr<QueueDiscItem> item);

private:
  /// The set of current buffer partition sizes
  std::vector<uint32_t> m_partitions;
  /// The max limit on the size of each partition
  std::vector<uint32_t> m_partitionLimits;
  /// Map a buffer ID to list of partition indicies, each index is checked in order for space
  std::map<uint32_t, std::vector<uint32_t>> m_bufIDMap;

  /// Traced callback: fired when a packet is enqueued into a partition
  TracedCallback<Ptr<const QueueDiscItem>, uint32_t partitionID > m_traceEnqueue;
  /// Traced callback: fired when a packet is dequeued from a partition
  TracedCallback<Ptr<const QueueDiscItem>, uint32_t partitionID > m_traceDequeue;

};

} // namespace ns3

#endif /* PIFO_TREE_BUFFER_H */

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

#ifndef PIFO_TREE_QDISC_H
#define PIFO_TREE_QDISC_H

#include "ns3/queue-disc.h"
#include "ns3/enq-pipeline.h"
#include "ns3/deq-pipeline.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * TODO (sibanez): detailed description ...
 * Scheduling is performed using a tree of PIFO queue discs.
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
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded

  /*
   * The following methods are all the same as the QueueDisc::* methods except they consume
   * an extra parameter deq_data, which indicates the node and PIFO to dequeue from.
   * TODO(sibanez): Is there a better way to do this than copy over the functions?
   */
  void Run (deq_data_t deq_data);
  void Restart (deq_data_t deq_data);
  Ptr<QueueDiscItem> DequeuePacket (deq_data_t deq_data);
  Ptr<QueueDiscItem> Dequeue (deq_data_t deq_data);

private:
  /// JSON file which speicifes the configuration of the Pifo Tree
  std::string m_pifoTreeJson;

  /// Store pointers to all PifoTreeNodes
  std::vector<Ptr<PifoTreeNode>> m_nodes;

  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  /**
   * \brief Helper method for DoDequeue()
   * \param deq_data specifies which node and PIFO to dequeue from
   */
  virtual Ptr<QueueDiscItem> DoDequeue (deq_data_t deq_data);



};

} // namespace ns3

#endif /* PIFO_TREE_QDISC_H */

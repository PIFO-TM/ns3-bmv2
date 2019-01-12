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
 *
 */

#include <fstream>

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/socket.h"
#include "json/json.h"
#include "pifo-tree-node.h"

namespace ns3 {

//
// PifoEntry implementation
//

PifoEntry::PifoEntry ()
{
  //TODO(sibanez): complete ...
}

uint32_t
PifoEntry::GetSize (void)
{
  return m_pktLen;
}

uint32_t
PifoEntry::GetPriority (void)
{
  return m_rank;
}

//
// PifoTreeNode implementation
//

NS_LOG_COMPONENT_DEFINE ("PifoTreeNode");

NS_OBJECT_ENSURE_REGISTERED (PifoTreeNode);

TypeId PifoTreeNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PifoTreeNode")
    .SetParent<Object> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PifoTreeNode> ()
    // TODO(sibanez): add attributes / trace sources
    .AddTraceSource ("EnqP4Var1",
                     "1st traced P4 variable for enqueue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_enqP4Var1),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("EnqP4Var2",
                     "2nd traced P4 variable for enqueue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_enqP4Var2),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("EnqP4Var3",
                     "3rd traced P4 variable for enqueue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_enqP4Var3),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("EnqP4Var4",
                     "4th traced P4 variable for enqueue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_enqP4Var4),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("DeqP4Var1",
                     "1st traced P4 variable for dequeue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_deqP4Var1),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("DeqP4Var2",
                     "2nd traced P4 variable for dequeue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_deqP4Var2),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("DeqP4Var3",
                     "3rd traced P4 variable for dequeue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_deqP4Var3),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("DeqP4Var4",
                     "4th traced P4 variable for dequeue logic",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_deqP4Var4),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("PacketsInNode",
                     "The number of packets in the node",
                     MakeTraceSourceAccessor (&PifoTreeNode::m_nPackets),
                     "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

// initialize static attributes
uint32_t PifoTreeNode::nodeID = 0;

PifoTreeNode::PifoTreeNode ()
{
  NS_LOG_FUNCTION (this);

  m_globalID = nodeID++;
  m_isLeaf = true;
  m_parent = 0;
  m_enqPipe = 0; 
  m_deqPipe = 0;
  m_nPackets = 0;
}

PifoTreeNode::~PifoTreeNode ()
{
  NS_LOG_FUNCTION (this);

  if (m_enqPipe)
    delete m_enqPipe;

  if (m_deqPipe)
    delete m_deqPipe;
}

bool
PifoTreeNode::AddEnqLogic (std::string enqJson, std::string enqCommands)
{
  NS_LOG_FUNCTION (this << enqJson);

  if (!m_enqPipe)
    {
      m_enqPipe = new EnqP4Pipe (enqJson);
      m_enqPipe->run_cli (enqCommands);
      return true;
    }
  else
    {
      NS_LOG_ERROR ("Enqueue logic has already been allocated for node " << m_globalID);
      return false;
    {
}

bool
PifoTreeNode::AddDeqLogic (std::string deqJson, std::string deqCommands)
{
  NS_LOG_FUNCTION (this << deqJson);

  if (!m_deqPipe)
    {
      m_deqPipe = new DeqP4Pipe (deqJson);
      m_deqPipe->run_cli (deqCommands);
      return true;
    }
  else
    {
      NS_LOG_ERROR ("Dequeue logic has already been allocated for node " << m_globalID);
      return false;
    {
}

bool
PifoTreeNode::AddPifos (int numPifos)
{
  NS_LOG_FUNCTION (this << numPifos);

  for (int i = 0; i < numPifos; i++)
    {
      // Add a new PIFO
      m_pifos.emplace_back ();
    }
  return true; 
}

bool
PifoTreeNode::AddParent (Ptr<PifoTreeNode> parent)
{
  NS_LOG_FUNCTION (this << parent);

  if (!m_parent)
    {
      m_parent = parent;
      return true;
    }
  else
    {
      NS_LOG_ERROR ("Node " << m_globalID << " already has a parent node. There can only be one!");
      return false;
    }
}

bool
PifoTreeNode::AddChild (Ptr<PifoTreeNode> child)
{
  NS_LOG_FUNCTION (this << child);

  m_isLeaf = false;
  uint32_t global_id = child->GetGlobalID ();
  uint8_t local_id = m_children.size ();
  m_children.push_back (child);

  // record the mapping between global and local IDs
  m_global2Local[global_id] = local_id;
  return true;
}

bool
PifoTreeNode::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);

  bool isRoot = (m_globalID == 0);
  if (isRoot && (m_pifos.size () > 1) && !m_deqPipe)
    {
      NS_LOG_ERROR ("The root node has multiple PIFOs and no dequeue logic has been provided");
      return false;
    }

  if (!isRoot && !m_parent)
    {
      NS_LOG_ERROR ("Node " << m_globalID << " is not the root node and it does not have a parent");
      return false;
    }

  // TODO(sibanez): additional conditions to check?

  return true;
}

uint8_t
PifoTreeNode::GetLocalNodeID (uint32_t global_node_id)
{
  NS_LOG_FUNCTION (this);

  // if global_node_id is invalid then the enqueue came from a node that is
  // not this node's child! Sould not be possible.
  NS_ASSERT (m_global2Local.cointains(global_node_id));
  return m_global2Local[global_node_id];
}

void
PifoTreeNode::InitEnqMeta (std_enq_meta_t& std_enq_meta)
{
  std_enq_meta.sched_meta.pkt_len = 0;
  std_enq_meta.sched_meta.flow_hash = 0;
  std_enq_meta.sched_meta.buf_id = 0;
  std_enq_meta.sched_meta.buf_size = 0;
  std_enq_meta.sched_meta.max_buf_size = 0;
  std_enq_meta.timestamp = Simulator::Now ().GetNanoSeconds ();
  std_enq_meta.is_leaf = m_isLeaf;
  std_enq_meta.child_node_id = 0;
  std_enq_meta.child_pifo_id = 0;
  // outputs
  std_enq_meta.rank = 0;
  std_enq_meta.pifo_id = 0;
  std_enq_meta.enq_delay = 0;
  std_enq_meta.tx_time = 0;
  std_enq_meta.tx_delta = 0;
  // tracedata
  std_enq_meta.trace_var1 = m_enqP4Var1;
  std_enq_meta.trace_var2 = m_enqP4Var2;
  std_enq_meta.trace_var3 = m_enqP4Var3;
  std_enq_meta.trace_var4 = m_enqP4Var4;
}

void
PifoTreeNode::InitDeqMeta (std_deq_meta_t& std_deq_meta)
{
  std_deq_meta.timestamp = Simulator::Now ().GetNanoSeconds ();
  std_deq_meta.is_leaf = m_isLeaf;

  // initialize PIFO metadata
  for (int = 0; i < MAX_NUM_PIFOS; i++)
    {
      std_deq_meta.pifo_is_empty[i]      = (i < m_pifos.size ()) ? m_pifos[i].empty () : true;
      std_deq_meta.pifo_last_deq_time[i] = (i < m_pifos.size ()) ? m_pifos[i].lastPopTime () : 0;
      std_deq_meta.pifo_child_node_id[i] = (i < m_pifos.size ()) ? m_pifos[i].top ().node_id : 0;
      std_deq_meta.pifo_child_pifo_id[i] = (i < m_pifos.size ()) ? m_pifos[i].top ().pifo_id : 0;
      std_deq_meta.pifo_rank[i]          = (i < m_pifos.size ()) ? m_pifos[i].top ().rank : 0;
      std_deq_meta.pifo_tx_time[i]       = (i < m_pifos.size ()) ? m_pifos[i].top ().tx_time : 0;
      std_deq_meta.pifo_tx_delta[i]      = (i < m_pifos.size ()) ? m_pifos[i].top ().tx_delta : 0;
      std_deq_meta.pifo_pkt_len[i]       = (i < m_pifos.size ()) ? m_pifos[i].top ().pkt_len : 0;
    }

  std_deq_meta.pifo_id = 0;
  std_deq_meta.deq_delay = 0;

  std_deq_meta.trace_var1 = m_deqP4Var1;
  std_deq_meta.trace_var2 = m_deqP4Var2;
  std_deq_meta.trace_var3 = m_deqP4Var3;
  std_deq_meta.trace_var4 = m_deqP4Var4;
}

bool
PifoTreeNode::Enqueue (Ptr<QueueDiscItem> item, sched_meta_t sched_meta)
{
  NS_LOG_FUNCTION (this);

  // TODO(sibanez): check m_lastEnqTime to make sure an enqueue has not already happened in this slot. If so, then reschedule enqueue for next ns.

  // allocate and initialize std_enq_meta
  std_enq_meta_t std_enq_meta;
  InitEnqMeta (std_enq_meta);
  std_enq_meta.sched_meta = sched_meta;
  std_enq_meta.child_node_id = 0;
  std_enq_meta.child_pifo_id = 0;

  // perform enqueue P4 processing
  m_enqPipe->process_pipeline (std_enq_meta);

  // update trace variables
  m_enqP4Var1 = std_enq_meta.trace_var1;
  m_enqP4Var2 = std_enq_meta.trace_var2;
  m_enqP4Var3 = std_enq_meta.trace_var3;
  m_enqP4Var4 = std_enq_meta.trace_var4;

  // check results of P4 processing
  uint32_t enq_delay = std_enq_meta.enq_delay;
  uint8_t pifo_id = std_enq_meta.pifo_id;
  NS_ASSERT (pifo_id < m_pifos.size ());

  // enqueue into PIFO
  m_pifos[pifo_id].emplace(item,
                           std_enq_meta.rank,
                           std_enq_meta.tx_time,
                           std_enq_meta.tx_delta,
                           sched_meta.pkt_len
                           );
  m_nPackets++;

  // perform the next enqueue operation
  bool result = EnqueueNext (enq_delay, pifo_id, sched_meta);
  return result;
}

bool
PifoTreeNode::Enqueue (uint32_t child_node_gid, uint8_t child_pifo_id, sched_meta_t sched_meta)
{
  NS_LOG_FUNCTION (this);

  // TODO(sibanez): check m_lastEnqTime to make sure an enqueue has not already happened in this slot. If so, then reschedule enqueue for next ns.

  // allocate and initialize std_enq_meta
  std_enq_meta_t std_enq_meta;
  InitEnqMeta (std_enq_meta);
  std_enq_meta.sched_meta = sched_meta;
  std_enq_meta.child_node_id = GetLocalNodeID (child_node_gid);
  std_enq_meta.child_pifo_id = child_pifo_id;

  // perform enqueue P4 processing
  m_enqPipe->process_pipeline (std_enq_meta);

  // update trace variables
  m_enqP4Var1 = std_enq_meta.trace_var1;
  m_enqP4Var2 = std_enq_meta.trace_var2;
  m_enqP4Var3 = std_enq_meta.trace_var3;
  m_enqP4Var4 = std_enq_meta.trace_var4;

  // check results of P4 processing
  uint32_t enq_delay = std_enq_meta.enq_delay;
  uint8_t pifo_id = std_enq_meta.pifo_id;
  NS_ASSERT (pifo_id < m_pifos.size ());

  // enqueue into PIFO
  m_pifos[pifo_id].emplace(std_enq_meta.child_node_id,
                           std_enq_meta.child_pifo_id,
                           std_enq_meta.rank,
                           std_enq_meta.tx_time,
                           std_enq_meta.tx_delta,
                           sched_meta.pkt_len
                           );
  m_nPackets++;

  // perform the next enqueue operation
  bool result = EnqueueNext (enq_delay, pifo_id, sched_meta);
  return result;
}

bool
PifoTreeNode::EnqueueNext (uint32_t enq_delay, uint8_t pifo_id, sched_meta_t sched_meta)
{
  NS_LOG_FUNCTION (this);

  bool result;
  if (m_parent == 0)
    {
      // this is the root node, no more enqueues to perform
      result = true;
    }
  else if (enq_delay > 0)
    {
      // delay the next enqueue operation
      Simulator::Schedule (Time (enq_delay), &PifoTreeNode::Enqueue, m_parent, m_globalID, pifo_id, sched_meta);
      result = true;
    }
  else
    {
      // perform the next enqueue immediately
      result = m_parent->Enqueue (m_globalID, pifo_id, sched_meta);
    }

  return result;
}

Ptr<QueueDiscItem>
PifoTreeNode::Dequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  // allocate and initialize deq metadata
  std_deq_meta_t std_deq_meta;
  InitDeqMeta (std_deq_meta);

  // perform P4 dequeue processing
  m_deqPipe->process_pipeline (std_deq_meta);

  // update trace variables
  m_deqP4Var1 = std_deq_meta.trace_var1;
  m_deqP4Var2 = std_deq_meta.trace_var2;
  m_deqP4Var3 = std_deq_meta.trace_var3;
  m_deqP4Var4 = std_deq_meta.trace_var4;

  uint8_t pifo_id = std_deq_meta.pifo_id;
  uint32_t deq_delay = std_deq_meta.deq_delay;

  // dequeue from the appropriate PIFO
  if (pifo_id >= m_pifos.length())
    {
      // don't dequeue anything
      item = 0;
    }
  else if (deq_delay > 0)
    {
      // schedule dequeue to try again in the future
      deq_data_t deq_data;
      deq_data.nodeID = m_globalID;
      deq_data.pifoID = 0xff; // indicates pifoID is unspecified so use dequeue logic
      Simulator::Schedule (Time (deq_delay), &PifoTreeQueueDisc::Run, m_qdisc, deq_data); 
      item = 0; // don't dequeue anything for now
    }
  else
    {
      item = DequeuePifo (pifo_id);
    }

  return item;
}

Ptr<QueueDiscItem>
PifoTreeNode::Dequeue (uint8_t pifo_id)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  if (pifo_id >= m_pifos.size ())
    {
      // if the provided pifo_id is invalid then we don't know which PIFO
      // to dequeue from so we will use the P4 dequeue logic to decide
      item = Dequeue ();
    }
  else
    {
      item = DequeuePifo (pifo_id);
    }

  return item;
}

Ptr<QueueDiscItem>
PifoTreeNode::DequeuePifo (uint8_t pifo_id)
{
  NS_LOG_FUNCTION (this);
  Ptr<QueueDiscItem> item;

  // NOTE: this method is called by one of the Dequeue () methods, both of which check
  // to make sure that the pifo_id is valid. Therefore, there's no need to check again

  if (!m_pifos[pifo_id].empty ())
    {
      // dequeue from the specified PIFO
      uint8_t child_node_id = m_pifos[pifo_id].top ().node_id;
      uint8_t child_pifo_id = m_pifos[pifo_id].top ().pifo_id;
      m_pifos[pifo_id].dequeue();
      m_nPackets--;
      if (!m_isLeaf)
        {
          // this is a non-leaf node so invoke the next dequeue operation
          NS_ASSERT (child_node_id < m_children.length());
          item = m_children[child_node_id]->Dequeue (child_pifo_id);
        }
    }
  else
    {
      // specified PIFO is empty
      item = 0;
    }

  return item;
}

} // namespace ns3

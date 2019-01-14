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
#include "pifo-tree-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PifoTreeQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (PifoTreeQueueDisc);

TypeId PifoTreeQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PifoTreeQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PifoTreeQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc.",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
  ;
  return tid;
}

PifoTreeQueueDisc::PifoTreeQueueDisc ()
  // TODO(sibanez): what should we use here?
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

PifoTreeQueueDisc::~PifoTreeQueueDisc ()
{
  NS_LOG_FUNCTION (this);

  delete m_classPipe;
}

bool
PifoTreeQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  // classification logic to determine buffer ID and leaf node ID
  std_class_meta_t std_class_meta;
  std_class_meta.flow_hash = item->Hash ();
  std_class_meta.buf_id = 0;
  std_class_meta.leaf_id = 0;
  // TODO(sibanez): initialize metadata
  m_p4ClassPipe->process_pipeline (std_class_meta);

  // Attempt to enqueue into the specified buffer
  if (!m_buffer.Enqueue(std_class_meta.buf_id, item))
    {
      NS_LOG_LOGIC ("Buffer " << std_class_meta.buf_id << " is full -- dropping packet");
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      return false;
    }

  // TODO(sibanez): initialize metadata ...
  sched_meta_t sched_meta;
  sched_meta.pkt_len = item->GetSize ();
  sched_meta.flow_hash = item->Hash ();
  sched_meta.buf_id = std_class_meta.buf_id;
  sched_meta.buf_size = 
  sched_meta.max_buf_size =
  // buffer enqueue was successful so enqueue into the specified leaf node
  // This operation will fail if provided ID is invalid
  bool retval = EnqueueLeaf (std_class_meta.leaf_id, item, sched_meta);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  if (!retval)
    {
      NS_LOG_ERROR ("Packet enqueue failed unexpectedly.");
    }

  return retval;
}

bool
PifoTreeQueueDisc::EnqueueBuffer (uint32_t bufID, Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (bufID < m_buffers.size (), "Provided bufID " << bufID << " is invalid.");

  if (m_buffers[bufID] + item->GetSize () > m_buffer_limits[bufID])
    {
      return false;
    }

  m_buffers[bufID] += item->GetSize ();
  return true;
}

bool
PifoTreeQueueDisc::EnqueueLeaf (uint32_t leafID, Ptr<QueueDiscItem> item, sched_meta_t sched_meta)
{
  NS_LOG_FUNCTION (this);

  if (leafID >= m_nodes.size ())
    {
      NS_LOG_ERROR ("Computed leaf node ID " << leafID << " is invalid");
      return false
    }
 
  bool ret = m_nodes[leafID].Enqueue (item, sched_meta);
  return ret;
}

Ptr<QueueDiscItem>
PifoTreeQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<PifoTreeItem> item;

  item = m_nodes[0].Dequeue();

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Dequeue ()) != 0)
        {
          NS_LOG_LOGIC ("Popped from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
PifoTreeQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  //TODO(sibanez): implement this ...

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
PifoTreeQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () != 0)
    {
      NS_LOG_ERROR ("PifoTreeQueueDisc needs no queue disc class");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("PifoTreeQueueDisc needs no packet filter");
      return false;
    }

  if (GetNInternalQueues () != 0)
    {
      NS_LOG_ERROR ("PifoTreeQueueDisc needs no internal queues");
      return false;
    }

  return true;
}

void
PifoTreeQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing PifoTreeQueueDisc params.");

  // create and initialize the Pifo Tree
  if (m_pifoTreeJson != "")
    {
      BuildPifoTree (m_pifoTreeJson);
    }

}

void
PifoTreeQueueDisc::ConfigClassification (Json::Value classLogic)
{
  NS_LOG_FUNCTION (this);

  std::string classJson = classLogic[0].asString ();
  std::string classCmds = classLogic[1].asString ();

  m_classPipe = new ClassificationP4Pipe (classJson);
  m_classPipe->run_cli (classCmds);
}

void
PifoTreeQueueDisc::ConfigNode (Json::Value jsonRoot, std::string param)
{
  NS_LOG_FUNCTION (this);

  Json::Value data = jsonRoot[param];

  bool ret;
  for (Json::Value::const_iterator itr = data.begin() ; itr != data.end() ; itr++)
    {
      nodeID = itr.key ().asInt ();
      NS_ASSERT (nodeID < m_nodes.size ());
      if (param == "enq-logic")
        {
          Json::Value enqJson = (*itr)[0];
          Json::Value enqCommands = (*itr)[1];
          ret = m_nodes[nodeID].AddEnqLogic (enqJson.asString (), enqCommands.asString ());
        }
      else if (param == "deq-logic")
        {
          Json::Value deqJson = (*itr)[0];
          Json::Value deqCommands = (*itr)[1];
          ret = m_nodes[nodeID].AddDeqLogic (deqJson.asString (), deqCommands.asString ());
        }
      else if (param == "num-pifos")
        {
          ret = m_nodes[nodeID].AddPifos ((*itr).asInt ());
        }
      else
        {
          NS_LOG_ERROR ("Unrecognized param " << param);
          ret = false;
        }
      NS_ASSERT (ret);
    } 
}

void
PifoTreeQueueDisc::BuildPifoTree (std::string pifoTreeJson)
{
  NS_LOG_FUNCTION (this << pifoTreeJson);

  /*
   Sample PIFO tree JSON file:

{
    "class-logic" : ["/path/to/class-logic.json", "/path/to/classCommands.txt"],
    "buffer-config" :
    {
        "num-bufIDs" : 3,
        "buffer-sizes" : [10000],
        "bufID-map" :
        {
            "0" : [0],
            "1" : [0],
            "2" : [0]
        }
    },
    "num-nodes" : 3,
    "tree" :
    {
        "0" : [1, 2]
    },
    "num-pifos" :
    {
        "0" : 1,
        "1" : 1,
        "2" : 1
    },
    "enq-logic" :
    {
        "0" : ["/path/to/enq-logic.json", "/path/to/enq-commands.txt"],
        "1" : ["/path/to/enq-logic.json", "/path/to/enq-commands.txt"],
        "2" : ["/path/to/enq-logic.json", "/path/to/enq-commands.txt"]
    },
    "deq-logic" :
    {
        "0" : ["/path/to/deq-logic.json", "/path/to/deq-commands.txt"],
        "1" : ["/path/to/deq-logic.json", "/path/to/deq-commands.txt"],
        "2" : ["/path/to/deq-logic.json", "/path/to/deq-commands.txt"]
    }
}
   */

    // read and parse the JSON file
    Json::Value jsonRoot;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::ifstream jsonFile (pifoTreeJson);
    bool ok = Json::parseFromStream(rbuilder, jsonFile, &jsonRoot, &errs);
    if (!ok)
      {
        NS_LOG_ERROR ("Error parsing json file: \n" << errs);
      }
    NS_ASSERT (ok);

    // initialize classification pipeline
    ConfigClassification (jsonRoot["class-logic"]);

    // configure buffer
    m_buffer.Configure(jsonRoot["buffer-sizes"]);

    // allocate nodes into std::vector
    for (int i = 0; i < jsonRoot["num-nodes"].asInt(); i++)
      {
        m_nodes.push_back(CreateObject<PifoTreeNode> ());
      }

    // iterate through enq-logic and add for each node (ensure each node has enq logic)
    ConfigNode (jsonRoot, "enq-logic");

    // iterate through deq-logic and add for each node
    ConfigNode (jsonRoot, "deq-logic");

    // add pifos to each node
    ConfigNode (jsonRoot, "num-pifos");

    bool ret;
    // add parent and children for each node
    Json::Value treeRoot = jsonRoot["tree"];
    for (Json::Value::const_iterator itr = treeRoot.begin() ; itr != treeRoot.end() ; itr++)
      {
        int parentID = itr.key ().asInt ();
        NS_ASSERT (parentID < m_nodes.size ());
        Json::Value children = (*itr);
        // add children
        for (int i = 0; i < children.size (); i++)
          {
            int childID = children[i].asInt ();
            NS_ASSERT (childID < m_nodes.size ());
            // add child to parent
            ret = m_nodes[parentID].AddChild (m_nodes[childID]);
            NS_ASSERT (ret);
            // add parent to child
            ret = m_nodes[childID].AddParent (m_nodes[parentID]);
            NS_ASSERT (ret);
          }
      }

    // check config of each node
    for (int i = 0; i < m_nodes.size (); i++)
      {
        ret = m_nodes[i].CheckConfig ();
        NS_ASSERT (ret);
      }
}


} // namespace ns3

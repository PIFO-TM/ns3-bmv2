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
#include <sstream>

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "pifo-tree-queue-disc.h"

namespace ns3 {

//
// PifoTreeQueueDisc Implementation
//

NS_LOG_COMPONENT_DEFINE ("PifoTreeQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (PifoTreeQueueDisc);

TypeId PifoTreeQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PifoTreeQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PifoTreeQueueDisc> ()
    // TODO(sibanez): add PifoTreeQueueDisc attributes and trace sources
    .AddAttribute ( "JsonFile", "The PifoTree config JSON file to use",
                    StringValue (""), MakeStringAccessor (&PifoTreeQueueDisc::GetJsonFile, &PifoTreeQueueDisc::SetJsonFile), MakeStringChecker ())
    .AddTraceSource ("P4ClassVar1",
                     "1st traced P4 classification variable",
                     MakeTraceSourceAccessor (&PifoTreeQueueDisc::m_p4ClassVar1),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("P4ClassVar2",
                     "2nd traced P4 classification variable",
                     MakeTraceSourceAccessor (&PifoTreeQueueDisc::m_p4ClassVar2),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("P4ClassVar3",
                     "3rd traced P4 classification variable",
                     MakeTraceSourceAccessor (&PifoTreeQueueDisc::m_p4ClassVar3),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("P4ClassVar4",
                     "4th traced P4 classification variable",
                     MakeTraceSourceAccessor (&PifoTreeQueueDisc::m_p4ClassVar4),
                     "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

PifoTreeQueueDisc::PifoTreeQueueDisc ()
  // We won't use any of the standard queue disc queueing infrastructure so the size policy doesn't matter
  : QueueDisc (), m_pifoTreeJson (""), m_jsonDir ("")
{
  NS_LOG_FUNCTION (this);
}

PifoTreeQueueDisc::~PifoTreeQueueDisc ()
{
  NS_LOG_FUNCTION (this);

  if (m_classPipe)
      delete m_classPipe;
}

std::string
PifoTreeQueueDisc::GetJsonFile (void) const
{
  NS_LOG_FUNCTION (this);
  return m_pifoTreeJson;
}

void
PifoTreeQueueDisc::SetJsonFile (std::string jsonFile)
{
  NS_LOG_FUNCTION (this << jsonFile);
  m_pifoTreeJson = jsonFile;
  // find the path to the jsonFile
  size_t found = jsonFile.find_last_of ("/");
  m_jsonDir = jsonFile.substr (0, found);
}

bool
PifoTreeQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  // classification logic to determine buffer ID and leaf node ID
  std_class_meta_t std_class_meta;
  std_class_meta.pkt_len = item->GetSize ();
  std_class_meta.flow_hash = item->Hash ();
  std_class_meta.timestamp = Simulator::Now ().GetNanoSeconds ();
  std_class_meta.buffer_id = 0;
  std_class_meta.leaf_id = 0;
  std_class_meta.trace_var1 = m_p4ClassVar1;
  std_class_meta.trace_var2 = m_p4ClassVar2;
  std_class_meta.trace_var3 = m_p4ClassVar3;
  std_class_meta.trace_var4 = m_p4ClassVar4;
  m_classPipe->process_pipeline (std_class_meta);

  // update classification trace variables
  m_p4ClassVar1 = std_class_meta.trace_var1;
  m_p4ClassVar2 = std_class_meta.trace_var2;
  m_p4ClassVar3 = std_class_meta.trace_var3;
  m_p4ClassVar4 = std_class_meta.trace_var4;

  // Attempt to enqueue into the specified buffer
  // TODO(sibanez): initialize scheduling metadata ...
  sched_meta_t sched_meta;
  sched_meta.pkt_len = item->GetSize ();
  sched_meta.flow_hash = item->Hash ();
  sched_meta.buffer_id = std_class_meta.buffer_id;
  // NOTE: the buffer's Enqueue() method will set these buffer related metadata fields
  sched_meta.partition_id = 0;
  sched_meta.partition_size = 0;
  sched_meta.partition_max_size = 0;
  if (!m_buffer.Enqueue(std_class_meta.buffer_id, item, sched_meta))
    {
      NS_LOG_LOGIC ("Buffer " << std_class_meta.buffer_id << " is full -- dropping packet");
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      return false;
    }

  // buffer enqueue was successful so enqueue into the specified leaf node
  // This operation will fail if provided leaf ID is invalid
  bool retval = EnqueueLeaf (std_class_meta.leaf_id, item, sched_meta);
  if (!retval)
    {
      NS_LOG_ERROR ("Failed to enqueue into PifoTree");
      DropBeforeEnqueue (item, PIFO_TREE_DROP);
      return false;
    }

  // NOTE: must invoke PacketEnqueued explicitly here because we are not using any NS3 internal queues
  PacketEnqueued (item);

  return retval;
}

bool
PifoTreeQueueDisc::EnqueueLeaf (uint32_t leafID, Ptr<QueueDiscItem> item, sched_meta_t sched_meta)
{
  NS_LOG_FUNCTION (this);

  if (leafID >= m_nodes.size ())
    {
      NS_LOG_ERROR ("Computed leaf node ID " << leafID << " is invalid");
      return false;
    }

  // NOTE: the node's EnqueueLeaf() method will verify that this is indeed a leaf node
  bool ret = m_nodes[leafID]->EnqueueLeaf (item, sched_meta);
  return ret;
}

Ptr<QueueDiscItem>
PifoTreeQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  uint32_t nodeID = 0;
  uint8_t pifoID = 0xff; // invalid pifoID so use dequeue logic
  return DoDequeue (nodeID, pifoID);
}

Ptr<QueueDiscItem>
PifoTreeQueueDisc::DoDequeue (Ptr<QueueDiscDeqData> deqData)
{
  NS_LOG_FUNCTION (this);

  // The deqData object is created in the PifoTreeNode Dequeue method
  Ptr<PifoTreeDeqData> pt_deqData = DynamicCast<PifoTreeDeqData> (deqData);
  return DoDequeue (pt_deqData->nodeID, pt_deqData->pifoID);
}

Ptr<QueueDiscItem>
PifoTreeQueueDisc::DoDequeue (uint32_t nodeID, uint8_t pifoID)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (nodeID < m_nodes.size (), "Attempted to dequeue from invalid node " << nodeID);

  Ptr<QueueDiscItem> item;
  sched_meta_t sched_meta;

  // dequeue starting with the specified node and PIFO
  // this call sets item and sched_meta
  bool ret = m_nodes[nodeID]->Dequeue (pifoID, item, sched_meta);

  if (ret)
    {
      // dequeue from the appropriate buffer partition
      m_buffer.Dequeue (sched_meta.partition_id, item);

      // NOTE: must invoke PacketDequeued explicitly here because we are not using any NS3 internal queues
      PacketDequeued (item);
    }

  return item;
}

// TODO(sibanez): Is this needed? Use default base class implementation for now
// Ptr<const QueueDiscItem>
// PifoTreeQueueDisc::DoPeek (void)
// {
//   NS_LOG_FUNCTION (this);
//   Ptr<const QueueDiscItem> item;
//   return item;
// }

bool
PifoTreeQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);

  // create and initialize the Pifo Tree
  if (m_pifoTreeJson != "")
    {
      BuildPifoTree (m_pifoTreeJson);
    }
  else
    {
      NS_LOG_ERROR ("PifoTree JSON file has not been configured");
      return false;
    }

  // check config of each node
  for (uint32_t i = 0; i < m_nodes.size (); i++)
    {
      if (!m_nodes[i]->CheckConfig ())
        {
          NS_LOG_ERROR ("Configuration check failed for node " << i);
          return false;
        }
    }

  if (GetNQueueDiscClasses () != 0)
    {
      NS_LOG_ERROR ("PifoTreeQueueDisc needs no queue disc classes");
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
}

void
PifoTreeQueueDisc::ConfigClassification (Json::Value& classLogic)
{
  NS_LOG_FUNCTION (this);

  std::string classJson = m_jsonDir + "/" + classLogic[0].asString ();
  std::string classCmds = m_jsonDir + "/" + classLogic[1].asString ();

  m_classPipe = new ClassificationP4Pipe (classJson);
  m_classPipe->run_cli (classCmds);
}

void
PifoTreeQueueDisc::ConfigNodes (Json::Value& jsonRoot, std::string param)
{
  NS_LOG_FUNCTION (this);

  Json::Value data = jsonRoot[param];

  bool ret;
  for (Json::Value::const_iterator itr = data.begin() ; itr != data.end() ; itr++)
    {
      int nodeID;
      std::stringstream (itr.key ().asString ()) >> nodeID;
      NS_ASSERT_MSG (nodeID < (int) m_nodes.size (), "Invalid node ID " << nodeID << " in JSON file");
      if (param == "enq-logic")
        {
          std::string enqJson = m_jsonDir + "/" + (*itr)[0].asString ();
          std::string enqCommands = m_jsonDir + "/" + (*itr)[1].asString ();
          ret = m_nodes[nodeID]->AddEnqLogic (enqJson, enqCommands);
        }
      else if (param == "deq-logic")
        {
          std::string deqJson = m_jsonDir + "/" + (*itr)[0].asString ();
          std::string deqCommands = m_jsonDir + "/" + (*itr)[1].asString ();
          ret = m_nodes[nodeID]->AddDeqLogic (deqJson, deqCommands);
        }
      else if (param == "num-pifos")
        {
          ret = m_nodes[nodeID]->AddPifos ((*itr).asInt ());
        }
      else
        {
          NS_LOG_ERROR ("Unrecognized param " << param);
          ret = false;
        }
      NS_ASSERT_MSG (ret, "Configuration failed for node " << nodeID);
    } 
}

void
PifoTreeQueueDisc::BuildPifoTree (std::string pifoTreeJson)
{
  NS_LOG_FUNCTION (this << pifoTreeJson);
  NS_LOG_INFO ("Building PifoTree from JSON config file");

  /*
   Sample PIFO tree JSON file:

{
    "class-logic" : ["/path/to/class-logic.json", "/path/to/classCommands.txt"],
    "buffer-config" :
    {
        "num-bufIDs" : 3,
        "partition-sizes" : [10000],
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
    m_buffer.Configure(jsonRoot["buffer-config"]);

    // allocate nodes into std::vector
    for (int i = 0; i < jsonRoot["num-nodes"].asInt(); i++)
      {
        m_nodes.push_back(CreateObject<PifoTreeNode> (this));
      }

    // iterate through enq-logic and add for each node (ensure each node has enq logic)
    ConfigNodes (jsonRoot, "enq-logic");

    // iterate through deq-logic and add for each node
    ConfigNodes (jsonRoot, "deq-logic");

    // add pifos to each node
    ConfigNodes (jsonRoot, "num-pifos");

    bool ret;
    // add parent and children for each node
    Json::Value treeRoot = jsonRoot["tree"];
    for (Json::Value::const_iterator itr = treeRoot.begin() ; itr != treeRoot.end() ; itr++)
      {
        int parentID;
        std::stringstream (itr.key ().asString ()) >> parentID;
        NS_ASSERT_MSG (parentID < (int) m_nodes.size (), "Invalid parent ID in PifoTree JSON file");
        Json::Value children = (*itr);
        // add children
        for (uint32_t i = 0; i < children.size (); i++)
          {
            int childID = children[i].asInt ();
            NS_ASSERT_MSG (childID < (int) m_nodes.size (), "Invalid child ID in PifoTree JSON file");
            // add child to parent
            ret = m_nodes[parentID]->AddChild (m_nodes[childID]);
            NS_ASSERT_MSG (ret, "Failed to add child to node " << parentID);
            // add parent to child
            ret = m_nodes[childID]->AddParent (m_nodes[parentID]);
            NS_ASSERT_MSG (ret, "Failed to add parent to node " << childID);
          }
      }
}


} // namespace ns3

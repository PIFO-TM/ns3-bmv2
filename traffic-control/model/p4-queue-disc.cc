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
 * Authors: Stephen Ibanez <sibanez@stanford.edu>
 */

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/object-factory.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/p4-pipeline.h"
#include "p4-queue-disc.h"
#include <algorithm>
#include <iterator>
#include <chrono>
#include <thread>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("P4QueueDisc");

NS_OBJECT_ENSURE_REGISTERED (P4QueueDisc);

TypeId P4QueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::P4QueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<P4QueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The max queue size",
                   QueueSizeValue (QueueSize ("500KB")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ( "JsonFile", "The bmv2 JSON file to use",
                    StringValue (""), MakeStringAccessor (&P4QueueDisc::GetJsonFile, &P4QueueDisc::SetJsonFile), MakeStringChecker ())
    .AddAttribute ( "CommandsFile", "A file with CLI commands to run on the P4 pipeline before starting the simulation",
                    StringValue (""), MakeStringAccessor (&P4QueueDisc::GetCommandsFile, &P4QueueDisc::SetCommandsFile), MakeStringChecker ())
    .AddAttribute ("QueueSizeBits",
                   "Number of bits to use to represent range of values for packet/queue size (up to 32)",
                   UintegerValue (16),
                   MakeUintegerAccessor (&P4QueueDisc::m_qSizeBits),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MeanPktSize",
                   "Average of packet size",
                   UintegerValue (500),
                   MakeUintegerAccessor (&P4QueueDisc::m_meanPktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LinkDelay", 
                   "The P4 queue disc link delay",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&P4QueueDisc::m_linkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("LinkBandwidth", 
                   "The P4 queue disc link bandwidth",
                   DataRateValue (DataRate ("1.5Mbps")),
                   MakeDataRateAccessor (&P4QueueDisc::m_linkBandwidth),
                   MakeDataRateChecker ())
    .AddAttribute ("QW",
                   "Queue weight related to the exponential weighted moving average (EWMA)",
                   DoubleValue (0.002),
                   MakeDoubleAccessor (&P4QueueDisc::m_qW),
                   MakeDoubleChecker <double> ())
    .AddTraceSource ("AvgQueueSize",
                     "The computed EWMA of the queue size",
                     MakeTraceSourceAccessor (&P4QueueDisc::m_qAvg),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

P4QueueDisc::P4QueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::SINGLE_CHILD_QUEUE_DISC, QueueSizeUnit::BYTES)
{
  NS_LOG_FUNCTION (this);
  m_p4Pipe = NULL;
}

P4QueueDisc::~P4QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  delete m_p4Pipe;
}

std::string
P4QueueDisc::GetJsonFile (void) const
{
  NS_LOG_FUNCTION (this);
  return m_jsonFile;
}

void
P4QueueDisc::SetJsonFile (std::string jsonFile)
{
  NS_LOG_FUNCTION (this << jsonFile);
  m_jsonFile = jsonFile;
}

std::string
P4QueueDisc::GetCommandsFile (void) const
{
  NS_LOG_FUNCTION (this);
  return m_commandsFile;
}

void
P4QueueDisc::SetCommandsFile (std::string commandsFile)
{
  NS_LOG_FUNCTION (this << commandsFile);
  m_commandsFile = commandsFile;
}

bool
P4QueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  //
  // Compute average queue size
  //
  uint32_t nQueued = GetCurrentSize ().GetValue (); 

  // simulate number of packets arrival during idle period
  uint32_t m = 0;
  if (m_idle == 1)
    {
      NS_LOG_DEBUG ("P4 Queue Disc is idle.");
      Time now = Simulator::Now ();
      m = uint32_t (m_ptc * (now - m_idleTime).GetSeconds ());
      m_idle = 0;
    }

  m_qAvg = Estimator (nQueued, m + 1, m_qAvg, m_qW);

  //
  // Initialize standard metadata
  //
  std_meta_t std_meta;
  std_meta.qdepth = MapSize ((double) nQueued);
  std_meta.avg_qdepth = MapSize (m_qAvg);
  std_meta.timestamp = Simulator::Now ().GetNanoSeconds ();
  std_meta.idle_time = m_idleTime.GetNanoSeconds ();
  std_meta.pkt_len = MapSize ((double) item->GetSize ());
  std_meta.l3_proto = item->GetProtocol ();
  std_meta.flow_hash = item->Hash (); //TODO(sibanez): include perturbation?
  std_meta.drop = false; 
  std_meta.mark = false;

  // perform P4 processing
  Ptr<Packet> new_packet = m_p4Pipe->process_pipeline(item->GetPacket(), std_meta);

  // replace the QueueDiscItem's packet
  item->SetPacket(new_packet);

  if (std_meta.drop)
    {
      NS_LOG_DEBUG ("Dropping packet because P4 program said to");
      DropBeforeEnqueue (item, P4_DROP);
      return false;
    }
  if (std_meta.mark)
    {
      NS_LOG_DEBUG ("Marking packet because P4 program said to");
      item->Mark();
    }

  bool retval = GetQueueDiscClass (0)->GetQueueDisc ()->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the child queue disc
  // because QueueDisc::AddQueueDiscClass sets the drop callback

  NS_LOG_LOGIC ("Number packets in queue disc " << GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets ());

  return retval;
}

uint32_t
P4QueueDisc::MapSize (double size)
{
  NS_LOG_FUNCTION (this << size);

  uint32_t maxSize = GetMaxSize ().GetValue();
  uint32_t result = (uint32_t) std::round (( ((double)size)/((double)maxSize) ) * ((1 << m_qSizeBits) - 1));

  NS_LOG_LOGIC ("Mapped size " << size << " into " << result);
  return result;
}

void
P4QueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing P4 Queue Disc params.");

  // create and initialize the P4 pipeline
  if (m_p4Pipe == NULL && m_jsonFile != "" && m_commandsFile != "")
    {
      m_p4Pipe = new SimpleP4Pipe(m_jsonFile);
      m_p4Pipe->run_cli (m_commandsFile);
    }

  m_ptc = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize);

  m_qAvg = 0.0;
  m_idle = 1;
  m_idleTime = NanoSeconds (0);

/*
 * If m_qW=0, set it to a reasonable value of 1-exp(-1/C)
 * This corresponds to choosing m_qW to be of that value for
 * which the packet time constant -1/ln(1-m)qW) per default RTT 
 * of 100ms is an order of magnitude more than the link capacity, C.
 *
 * If m_qW=-1, then the queue weight is set to be a function of
 * the bandwidth and the link propagation delay.  In particular, 
 * the default RTT is assumed to be three times the link delay and 
 * transmission delay, if this gives a default RTT greater than 100 ms. 
 *
 * If m_qW=-2, set it to a reasonable value of 1-exp(-10/C).
 */
  if (m_qW == 0.0)
    {
      m_qW = 1.0 - std::exp (-1.0 / m_ptc);
    }
  else if (m_qW == -1.0)
    {
      double rtt = 3.0 * (m_linkDelay.GetSeconds () + 1.0 / m_ptc);

      if (rtt < 0.1)
        {
          rtt = 0.1;
        }
      m_qW = 1.0 - std::exp (-1.0 / (10 * rtt * m_ptc));
    }
  else if (m_qW == -2.0)
    {
      m_qW = 1.0 - std::exp (-10.0 / m_ptc);
    }

  NS_LOG_DEBUG ("\tm_linkDelay " << m_linkDelay.GetSeconds ()
                << "; m_linkBandwidth " << m_linkBandwidth.GetBitRate ()
                << "; m_qW " << m_qW
                << "; m_ptc " << m_ptc
                );
}

// Compute the average queue size
double
P4QueueDisc::Estimator (uint32_t nQueued, uint32_t m, double qAvg, double qW)
{
  NS_LOG_FUNCTION (this << nQueued << m << qAvg << qW);

  double newAve = qAvg * std::pow (1.0 - qW, m);
  newAve += qW * nQueued;

  return newAve;
}

Ptr<QueueDiscItem>
P4QueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets() == 0)
    {
      NS_LOG_LOGIC ("Queue empty");
      m_idle = 1;
      m_idleTime = Simulator::Now ();

      return 0;
    }
  else
    {
      Ptr<QueueDiscItem> item;
      item = GetQueueDiscClass (0)->GetQueueDisc ()->Dequeue ();

      NS_LOG_LOGIC ("Popped from qdisc: " << item);
      NS_LOG_LOGIC ("Number packets in qdisc: " << GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets ());
      return item;
    }
}

Ptr<const QueueDiscItem>
P4QueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Peek ()) != 0)
    {
      NS_LOG_LOGIC ("Peeked from qdisc: " << item);
      NS_LOG_LOGIC ("Number packets band: " << GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets ());
      return item;
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
P4QueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("P4QueueDisc cannot have internal queues");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("P4QueueDisc cannot have any packet filters");
      return false;
    }

  if (GetNQueueDiscClasses () == 0)
    {
      // create 1 fifo queue disc
      ObjectFactory factory;
      factory.SetTypeId ("ns3::FifoQueueDisc");
      Ptr<QueueDisc> qd = factory.Create<QueueDisc> ();

      if (!qd->SetMaxSize (GetMaxSize ()))
        {
          NS_LOG_ERROR ("Cannot set the max size of the child queue disc to that of P4QueueDisc");
          return false;
        }
      qd->Initialize ();
      Ptr<QueueDiscClass> c = CreateObject<QueueDiscClass> ();
      c->SetQueueDisc (qd);
      AddQueueDiscClass (c);
    }

  if (GetNQueueDiscClasses () != 1)
    {
      NS_LOG_ERROR ("P4QueueDisc requires exactly 1 class");
      return false;
    }

  if (m_jsonFile == "")
    {
      NS_LOG_ERROR ("P4QueueDisc is not configured with a JSON file");
      return false;
    }

  if (m_commandsFile == "")
    {
      NS_LOG_ERROR ("P4QueueDisc is not configured with a CLI commands file");
      return false;
    }

  return true;
}

} // namespace ns3

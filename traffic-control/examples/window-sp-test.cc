/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

/** Network topology
 *
 *        100Mb/s, 2ms                            100Mb/s, 2ms
 * n(0)----------------|                      |---------------n(N)
 *  .                  |     10Mbps/s, 20ms   |                .
 *  .                  n(2N)------------------n(2N+1)          .
 *  .     100Mb/s, 2ms |                      |   100Mb/s, 2ms .
 * n(N-1)--------------|                      |---------------n(2N-1)
 *
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WindowStrictPriorityTest");

// The times
double global_start_time = 0.0;
double global_stop_time = 4.0;  // default (configurable by cmd line)
double sink_start_time;
double sink_stop_time; 
// double client_start_time;
double client_stop_time;

bool printStats = true;
bool writeAppBytes = false;

NodeContainer sources;
NodeContainer sinks;
NodeContainer routers;
QueueDiscContainer queueDiscs;

std::vector<double> appStartTimes = {0.1, 0.2, 0.3};
std::vector<int> initFlowRates = {1, 1, 10}; // Mbps
std::vector<int> finalFlowRates = {5, 5, 10}; // Mbps
std::vector<double> rateChangeTimes = {1.0, 2.0, 0.0};
std::string pathOut = ".";
std::string jsonFile = "";
uint32_t numPartitions = 3;
int numApps = 3;
std::string bnLinkDataRate = "10Mbps";
std::string bnLinkDelay = "20ms";
std::string defaultDataRate = "100Mbps";
std::string defaultDelay = "2ms";
uint32_t meanPktSize = 100;
// the time window over which to compute rate measurements
std::string rateWindow = "10ms";

AsciiTraceHelper asciiTraceHelper;
// each app has a file that contains its tx/rx rate over time
std::vector<Ptr<OutputStreamWrapper>> txRates;
std::vector<Ptr<OutputStreamWrapper>> rxRates;

// to track the number of bytes transmitted and received by each
// app over a time window (used to compute rates)
std::vector<int> txBytes;
std::vector<int> rxBytes;

// to track the occupancy of each partition
std::vector<uint32_t> partitions; 
std::vector<Ptr<OutputStreamWrapper>> qsizeStreams;

// to track drops per partition
std::vector<Ptr<OutputStreamWrapper>> dropStreams;

void InitGlobals ()
{
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 1.0;
  //client_start_time = sink_start_time + 0.2;
  client_stop_time = global_stop_time - 1.0;

  for (int i = 0; i < numApps; i++)
    {
      // initialize bytes to 0
      txBytes.push_back (0);
      rxBytes.push_back (0);
      // initialize rate files
      txRates.push_back (asciiTraceHelper.CreateFileStream (pathOut + "/tx-rate-" + std::to_string(i) + ".plotme")); 
      rxRates.push_back (asciiTraceHelper.CreateFileStream (pathOut + "/rx-rate-" + std::to_string(i) + ".plotme")); 
    }

  for (uint32_t i = 0; i < numPartitions; i++)
    {
      partitions.push_back (0);
      qsizeStreams.push_back (asciiTraceHelper.CreateFileStream (pathOut + "/queue-" + std::to_string(i) +"-size.plotme"));
      dropStreams.push_back (asciiTraceHelper.CreateFileStream (pathOut + "/queue-" + std::to_string(i) +"-drops.plotme"));
    }
}

// convert bytes/sec to Kbps
double
BpsToKbps (double bytesps)
{
  return bytesps*8e-3;
}

// Record tx / rx rates for each application
void
RecordRates ()
{
  Ptr<OutputStreamWrapper> stream;
  double rate;
  double window = Time (rateWindow).GetSeconds ();
  double now = Simulator::Now ().GetSeconds ();
  if (now >= window)
    {
      // Record the tx & rx rate for each application over the last time window
      for (int i = 0; i < numApps; i++)
        {
          // record tx rate
          rate = ((double)txBytes[i])/window;
          txBytes[i] = 0; // reset bytes
          stream = txRates[i];
          *stream->GetStream () << (now - window/2) << " " << BpsToKbps(rate) << std::endl;
          // record rx rate
          rate = ((double)rxBytes[i])/window;
          rxBytes[i] = 0; // reset bytes
          stream = rxRates[i];
          *stream->GetStream () << (now - window/2) << " " << BpsToKbps(rate) << std::endl;
        }
    }

  // reschedule this function to run again later
  Simulator::Schedule (Time (rateWindow), &RecordRates);
}

//
// The trace sink call back functions
//
void
BufferEnqueueTrace (Ptr<const QueueDiscItem> item, uint32_t partitionID)
{
  NS_ASSERT_MSG (partitionID < numPartitions, "Invalid partitionID? Does the PifoTreeJson agree with numPartitions in this test script?");
  // update the buffer partition size
  partitions[partitionID] += item->GetSize ();
  Ptr<OutputStreamWrapper> stream = qsizeStreams[partitionID];
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << partitions[partitionID] << std::endl;
}

void
BufferDequeueTrace (Ptr<const QueueDiscItem> item, uint32_t partitionID)
{
  NS_ASSERT_MSG (partitionID < numPartitions, "Invalid partitionID? Does the PifoTreeJson agree with numPartitions in this test script?");
  // update the buffer partition size
  partitions[partitionID] -= item->GetSize ();
  Ptr<OutputStreamWrapper> stream = qsizeStreams[partitionID];
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << partitions[partitionID] << std::endl;
}

void
BufferDropTrace (Ptr<const QueueDiscItem> item, uint32_t bufferID)
{
  NS_ASSERT_MSG (bufferID < numPartitions, "Invalid bufferID? Does the PifoTreeJson agree with numPartitions in this test script?");
  Ptr<OutputStreamWrapper> stream = dropStreams[bufferID];
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << 0 << std::endl; 
}

void
TcDropTrace (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << 0 << std::endl;
}

void
TxTrace (Ptr<OutputStreamWrapper> stream, int appId, Ptr<const Packet> pkt)
{
  txBytes[appId] += pkt->GetSize();
  if (writeAppBytes)
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << txBytes[appId] << std::endl;
}

void
RxTrace (Ptr<OutputStreamWrapper> stream, int appId, Ptr<const Packet> pkt, const Address &address)
{
  rxBytes[appId] += pkt->GetSize();
  if (writeAppBytes)
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << rxBytes[appId] << std::endl;
}

//
// Configure qdisc parameters
//
void
configQdisc (TrafficControlHelper &tchQdisc)
{
  if (jsonFile == "")
    {
      NS_LOG_ERROR("PifoTreeJSON file has not been configured");
    }

  // P4 queue disc params
  NS_LOG_INFO("Set PifoTree queue disc params");
  Config::SetDefault ("ns3::PifoTreeQueueDisc::JsonFile", StringValue (jsonFile));

  tchQdisc.SetRootQueueDisc ("ns3::PifoTreeQueueDisc");
}

//
// Setup the topology and routing
//
void
SetupTopo ()
{
  NS_LOG_INFO ("Create nodes");
  sources.Create (numApps);
  sinks.Create (numApps);
  routers.Create (2);

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (sources);
  internet.Install (sinks);
  internet.Install (routers);

  TrafficControlHelper tchQdisc;
  configQdisc(tchQdisc);

  TrafficControlHelper tchPfifo;
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue ("1000p"));

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;

  Ptr<Node> r0 = routers.Get (0);
  Ptr<Node> r1 = routers.Get (1);
  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue (defaultDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (defaultDelay));

  std::vector<NetDeviceContainer> srcDevs;
  std::vector<NetDeviceContainer> sinkDevs;

  // connect sources to r0
  for (int i = 0; i < numApps; i++)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (sources.Get (i), r0));
      tchPfifo.Install (devs);
      srcDevs.push_back (devs);
    }

  // connect sinks to r1 
  for (int i = 0; i < numApps; i++)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (sinks.Get (i), r1));
      tchPfifo.Install (devs);
      sinkDevs.push_back (devs);
    }

  // connect routers
  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue (bnLinkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (bnLinkDelay));
  NetDeviceContainer devr0r1 = p2p.Install (routers);
  // only bottleneck link has selected queue disc implemenation
  queueDiscs = tchQdisc.Install (devr0r1.Get (0));
  tchPfifo.Install (devr0r1.Get (1));

  NS_LOG_INFO ("Assign IP Addresses");
  Ipv4AddressHelper ipv4;

  // Assign IP addresses for sources <--> r0
  for (int i = 0; i < numApps; i++)
    {
      std::string base = "10.1." + std::to_string(i+1) + ".0";
      ipv4.SetBase (base.c_str(), "255.255.255.0");
      ipv4.Assign (srcDevs[i]);
    }

  // Assign IP addresses for sinks <--> r1
  for (int i = 0; i < numApps; i++)
    {
      std::string base = "10.2." + std::to_string(i+1) + ".0";
      ipv4.SetBase (base.c_str(), "255.255.255.0");
      ipv4.Assign (sinkDevs[i]);
    }

  // Assign IP addresses for backbone
  ipv4.SetBase ("10.3.1.0", "255.255.255.0");
  ipv4.Assign (devr0r1);

  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void
ChangeAppRate (Ptr<OnOffApplication> app, int rate)
{
  app->SetAttribute("DataRate", DataRate (std::to_string(rate) + "Mbps"));
}

void
SetupApps ()
{
  Address dstAddr;
  std::string dstAddrStr;
  ApplicationContainer app;
  uint16_t port = 9; // Discard port (RFC 863)
  std::string sendRate;
  for (int i = 0; i < numApps; i++)
    {
      // compute rate for this app
      sendRate = std::to_string(initFlowRates[i]) + "Mbps";
      // Install Sources
      dstAddrStr = "10.2." + std::to_string (i+1) + ".1";
      dstAddr = Address (InetSocketAddress (Ipv4Address (dstAddrStr.c_str()), port));
      OnOffHelper onoff ("ns3::UdpSocketFactory", dstAddr);
      onoff.SetConstantRate (DataRate (sendRate), meanPktSize);
      app = onoff.Install (sources.Get (i));
      app.Start (Seconds (appStartTimes[i] + sink_start_time));
      app.Stop (Seconds (client_stop_time));

      // schedule rate changes for each source app
      if (rateChangeTimes[i] > 0)
        {
          Simulator::Schedule (Seconds (rateChangeTimes[i]), &ChangeAppRate, DynamicCast<OnOffApplication>(app.Get(0)), finalFlowRates[i]);
        }

      // Install Sinks
      PacketSinkHelper sink ("ns3::UdpSocketFactory", dstAddr);
      app = sink.Install (sinks.Get (i));
      app.Start (Seconds (sink_start_time));
      app.Stop (Seconds (sink_stop_time));
    }
}

//
// Configure tracing
//
void
ConfigTracing ()
{

  Ptr<QueueDisc> qdisc = queueDiscs.Get (0);
  //
  // Configure tracing of the Instantaneous queue size
  //
  qdisc->TraceConnectWithoutContext ("BufferEnqueue", MakeCallback (&BufferEnqueueTrace));
  qdisc->TraceConnectWithoutContext ("BufferDequeue", MakeCallback (&BufferDequeueTrace));
  qdisc->TraceConnectWithoutContext ("BufferDrop", MakeCallback (&BufferDropTrace));
  //
  // Configure tracing of packet drops
  //
  Ptr<OutputStreamWrapper> dropStream = asciiTraceHelper.CreateFileStream (pathOut + "/drop-times.plotme");
  qdisc->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&TcDropTrace, dropStream));
  //
  // Configure tracing of traffic sources
  //
  for (int i = 0; i < numApps; i++)
    {
      Ptr<Node> node = sources.Get (i);
      std::string path = "/NodeList/" +
                         std::to_string(node->GetId()) +
                         "/ApplicationList/0/$ns3::OnOffApplication/Tx";
      Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream (pathOut + "/tx-bytes-" + std::to_string(i) + ".plotme");
      Config::ConnectWithoutContext (path, MakeBoundCallback (&TxTrace, txStream, i));
    }
  //
  // Configure tracing of traffic sinks
  //
  for (int i = 0; i < numApps; i++)
    {
      Ptr<Node> node = sinks.Get (i);
      std::string path = "/NodeList/" +
                         std::to_string(node->GetId()) +
                         "/ApplicationList/0/$ns3::PacketSink/Rx";
      Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream (pathOut + "/rx-bytes-" + std::to_string(i) + ".plotme");
      Config::ConnectWithoutContext (path, MakeBoundCallback (&RxTrace, rxStream, i));
    }

  // Start tracking application rates
  Simulator::ScheduleNow (&RecordRates);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("WindowStrictPriorityTest", LOG_LEVEL_INFO);

  // Configuration and command line parameter parsing
  CommandLine cmd;
  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("jsonFile", "Path to the desired bmv2 JSON file", jsonFile);
  cmd.AddValue ("numApps", "Number of CBR sources/sinks to use", numApps);
  cmd.AddValue ("writeAppBytes", "Write the tx/rx bytes for each app", writeAppBytes);
  cmd.AddValue ("duration", "Write the tx/rx bytes for each app", global_stop_time);
  cmd.Parse (argc, argv);

  InitGlobals ();

  SetupTopo ();

  SetupApps ();

  ConfigTracing ();

  // Install flow monitor
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (sink_stop_time));
  Simulator::Run ();

  // Log flow monitor output
  std::stringstream stmp;
  stmp << pathOut << "/flowmon.txt";
  flowmon->SerializeToXmlFile (stmp.str ().c_str (), false, false);

  if (printStats)
    {
      QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();
      std::cout << "*** Stats from PifoTree queue disc ***" << std::endl;
      std::cout << st << std::endl;
    }

  Simulator::Destroy ();

  return 0;
}

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
 * Authors: Marcos Talau <talau@users.sourceforge.net>
 *          Duy Nguyen <duy@soe.ucsc.edu>
 * Modified by:   Pasquale Imputato <p.imputato@gmail.com>
 * Modified by:   Stephen Ibanez <sibanez@stanford.edu>
 *
 */

/**
 * These tests where originally designed to test RED implementations, but have
 * been refactored to be more general. Compare to red-tests.cc
 */

/** Network topology
 *
 *    10Mb/s, 2ms                            10Mb/s, 4ms
 * n0--------------|                    |---------------n4
 *                 |   1.5Mbps/s, 20ms  |
 *                 n2------------------n3
 *    10Mb/s, 3ms  |                    |    10Mb/s, 5ms
 * n1--------------|                    |---------------n5
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

NS_LOG_COMPONENT_DEFINE ("QdiscCongestion");

uint32_t checkTimes;
double avgQueueSize;

// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;

NodeContainer n0n2;
NodeContainer n1n2;
NodeContainer n2n3;
NodeContainer n3n4;
NodeContainer n3n5;

Ipv4InterfaceContainer i0i2;
Ipv4InterfaceContainer i1i2;
Ipv4InterfaceContainer i2i3;
Ipv4InterfaceContainer i3i4;
Ipv4InterfaceContainer i3i5;

std::stringstream filePlotQueue;
std::stringstream filePlotQueueAvg;

std::string jsonFile = "";
std::string commandsFile = "";
uint32_t qSizeBits = 16;
uint32_t testNum;
std::string bnLinkDataRate = "1.5Mbps";
std::string bnLinkDelay = "20ms";
std::string maxQueueSize = "500KB";
uint32_t meanPktSize = 500;
double qW = 0.002;

void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetCurrentSize ().GetValue ();

  avgQueueSize += qSize;
  checkTimes++;

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::ofstream fPlotQueue (filePlotQueue.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();

  std::ofstream fPlotQueueAvg (filePlotQueueAvg.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueueAvg << Simulator::Now ().GetSeconds () << " " << avgQueueSize / checkTimes << std::endl;
  fPlotQueueAvg.close ();
}

void
BuildAppsTest ()
{
  if ( (testNum == 1) || (testNum == 3) )
    {
      // SINK is in the right side
      uint16_t port = 50000;
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApp = sinkHelper.Install (n3n4.Get (1));
      sinkApp.Start (Seconds (sink_start_time));
      sinkApp.Stop (Seconds (sink_stop_time));

      // Connection one
      // Clients are in left side
      /*
       * Create the OnOff applications to send TCP to the server
       * onoffhelper is a client that send data to TCP destination
       */
      OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", Address ());
      clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      clientHelper1.SetAttribute 
        ("DataRate", DataRateValue (DataRate ("10Mb/s")));
      clientHelper1.SetAttribute 
        ("PacketSize", UintegerValue (1000));

      ApplicationContainer clientApps1;
      AddressValue remoteAddress
        (InetSocketAddress (i3i4.GetAddress (1), port));
      clientHelper1.SetAttribute ("Remote", remoteAddress);
      clientApps1.Add (clientHelper1.Install (n0n2.Get (0)));
      clientApps1.Start (Seconds (client_start_time));
      clientApps1.Stop (Seconds (client_stop_time));

      // Connection two
      OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", Address ());
      clientHelper2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      clientHelper2.SetAttribute 
        ("DataRate", DataRateValue (DataRate ("10Mb/s")));
      clientHelper2.SetAttribute 
        ("PacketSize", UintegerValue (1000));

      ApplicationContainer clientApps2;
      clientHelper2.SetAttribute ("Remote", remoteAddress);
      clientApps2.Add (clientHelper2.Install (n1n2.Get (0)));
      clientApps2.Start (Seconds (3.0));
      clientApps2.Stop (Seconds (client_stop_time));
    }
  else // 4
    {
      // SINKs
      // #1
      uint16_t port1 = 50001;
      Address sinkLocalAddress1 (InetSocketAddress (Ipv4Address::GetAny (), port1));
      PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", sinkLocalAddress1);
      ApplicationContainer sinkApp1 = sinkHelper1.Install (n3n4.Get (1));
      sinkApp1.Start (Seconds (sink_start_time));
      sinkApp1.Stop (Seconds (sink_stop_time));
      // #2
      uint16_t port2 = 50002;
      Address sinkLocalAddress2 (InetSocketAddress (Ipv4Address::GetAny (), port2));
      PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", sinkLocalAddress2);
      ApplicationContainer sinkApp2 = sinkHelper2.Install (n3n5.Get (1));
      sinkApp2.Start (Seconds (sink_start_time));
      sinkApp2.Stop (Seconds (sink_stop_time));
      // #3
      uint16_t port3 = 50003;
      Address sinkLocalAddress3 (InetSocketAddress (Ipv4Address::GetAny (), port3));
      PacketSinkHelper sinkHelper3 ("ns3::TcpSocketFactory", sinkLocalAddress3);
      ApplicationContainer sinkApp3 = sinkHelper3.Install (n0n2.Get (0));
      sinkApp3.Start (Seconds (sink_start_time));
      sinkApp3.Stop (Seconds (sink_stop_time));
      // #4
      uint16_t port4 = 50004;
      Address sinkLocalAddress4 (InetSocketAddress (Ipv4Address::GetAny (), port4));
      PacketSinkHelper sinkHelper4 ("ns3::TcpSocketFactory", sinkLocalAddress4);
      ApplicationContainer sinkApp4 = sinkHelper4.Install (n1n2.Get (0));
      sinkApp4.Start (Seconds (sink_start_time));
      sinkApp4.Stop (Seconds (sink_stop_time));

      // Connection #1
      /*
       * Create the OnOff applications to send TCP to the server
       * onoffhelper is a client that send data to TCP destination
       */
      OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", Address ());
      clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      clientHelper1.SetAttribute 
        ("DataRate", DataRateValue (DataRate ("10Mb/s")));
      clientHelper1.SetAttribute 
        ("PacketSize", UintegerValue (1000));

      ApplicationContainer clientApps1;
      AddressValue remoteAddress1
        (InetSocketAddress (i3i4.GetAddress (1), port1));
      clientHelper1.SetAttribute ("Remote", remoteAddress1);
      clientApps1.Add (clientHelper1.Install (n0n2.Get (0)));
      clientApps1.Start (Seconds (client_start_time));
      clientApps1.Stop (Seconds (client_stop_time));

      // Connection #2
      OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", Address ());
      clientHelper2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      clientHelper2.SetAttribute 
        ("DataRate", DataRateValue (DataRate ("10Mb/s")));
      clientHelper2.SetAttribute 
        ("PacketSize", UintegerValue (1000));

      ApplicationContainer clientApps2;
      AddressValue remoteAddress2
        (InetSocketAddress (i3i5.GetAddress (1), port2));
      clientHelper2.SetAttribute ("Remote", remoteAddress2);
      clientApps2.Add (clientHelper2.Install (n1n2.Get (0)));
      clientApps2.Start (Seconds (2.0));
      clientApps2.Stop (Seconds (client_stop_time));

      // Connection #3
      OnOffHelper clientHelper3 ("ns3::TcpSocketFactory", Address ());
      clientHelper3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      clientHelper3.SetAttribute 
        ("DataRate", DataRateValue (DataRate ("10Mb/s")));
      clientHelper3.SetAttribute 
        ("PacketSize", UintegerValue (1000));

      ApplicationContainer clientApps3;
      AddressValue remoteAddress3
        (InetSocketAddress (i0i2.GetAddress (0), port3));
      clientHelper3.SetAttribute ("Remote", remoteAddress3);
      clientApps3.Add (clientHelper3.Install (n3n4.Get (1)));
      clientApps3.Start (Seconds (3.5));
      clientApps3.Stop (Seconds (client_stop_time));

      // Connection #4
      OnOffHelper clientHelper4 ("ns3::TcpSocketFactory", Address ());
      clientHelper4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      clientHelper4.SetAttribute 
        ("DataRate", DataRateValue (DataRate ("40b/s")));
      clientHelper4.SetAttribute 
        ("PacketSize", UintegerValue (5 * 8)); // telnet

      ApplicationContainer clientApps4;
      AddressValue remoteAddress4
        (InetSocketAddress (i1i2.GetAddress (0), port4));
      clientHelper4.SetAttribute ("Remote", remoteAddress4);
      clientApps4.Add (clientHelper4.Install (n3n5.Get (1)));
      clientApps4.Start (Seconds (1.0));
      clientApps4.Stop (Seconds (client_stop_time));
    }
}

void
configQdisc (std::string qdiscSelection, TrafficControlHelper &tchQdisc)
{
  if (qdiscSelection == "red")
    {
      // RED params
      NS_LOG_INFO ("Set RED params");
      Config::SetDefault ("ns3::RedQueueDisc::MaxSize", StringValue ("500KB"));
      Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (meanPktSize));
      Config::SetDefault ("ns3::RedQueueDisc::Wait", BooleanValue (true));
      Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
      Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (qW));
      Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (5 * meanPktSize));
      Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (15 * meanPktSize));
      Config::SetDefault ("ns3::RedQueueDisc::LinkBandwidth", StringValue (bnLinkDataRate));
      Config::SetDefault ("ns3::RedQueueDisc::LinkDelay", StringValue (bnLinkDelay));
      //Config::SetDefault ("ns3::RedQueueDisc::Ns1Compat", BooleanValue (true));

      if (testNum == 3) // test like 1, but with bad params
        {
          Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (10 * meanPktSize));
          Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (0.003));
        }

      tchQdisc.SetRootQueueDisc ("ns3::RedQueueDisc");
    }
  else if (qdiscSelection == "p4")
    {
      if (jsonFile == "" || commandsFile == "")
        {
          NS_LOG_ERROR("Using P4 queue disc, but JSON file or commands file is unconfigured");
        }

      // P4 queue disc params
      NS_LOG_INFO("Set P4 queue disc params");
      Config::SetDefault ("ns3::P4QueueDisc::MaxSize", StringValue (maxQueueSize));
      Config::SetDefault ("ns3::P4QueueDisc::JsonFile", StringValue (jsonFile));
      Config::SetDefault ("ns3::P4QueueDisc::CommandsFile", StringValue (commandsFile));
      Config::SetDefault ("ns3::P4QueueDisc::QueueSizeBits", UintegerValue (qSizeBits));
      Config::SetDefault ("ns3::P4QueueDisc::QW", DoubleValue (qW));
      Config::SetDefault ("ns3::P4QueueDisc::MeanPktSize", UintegerValue (meanPktSize));
      Config::SetDefault ("ns3::P4QueueDisc::LinkBandwidth", StringValue (bnLinkDataRate));
      Config::SetDefault ("ns3::P4QueueDisc::LinkDelay", StringValue (bnLinkDelay));

      tchQdisc.SetRootQueueDisc ("ns3::P4QueueDisc");
    }
  else
    {
      NS_LOG_ERROR("Unrecognized qdisc selection: " << qdiscSelection);
    }

}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("QdiscCongestion", LOG_LEVEL_INFO);

  std::string qdiscSelection = "";

  std::string pathOut;
  bool writeForPlot = false;
  bool writePcap = false;
  bool flowMonitor = false;

  bool printStats = true;

  global_start_time = 0.0;
  global_stop_time = 11; 
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 3.0;
  client_start_time = sink_start_time + 0.2;
  client_stop_time = global_stop_time - 2.0;

  // Configuration and command line parameter parsing
  testNum = 1;
  // Will only save in the directory if enable opts below
  pathOut = "."; // Current directory
  CommandLine cmd;
  cmd.AddValue ("qdisc", "Which qdisc implementation to run: red, p4", qdiscSelection);
  cmd.AddValue ("testNumber", "Run test 1, 3, 4", testNum);
  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("writeForPlot", "<0/1> to write results for plot (gnuplot)", writeForPlot);
  cmd.AddValue ("writePcap", "<0/1> to write results in pcapfile", writePcap);
  cmd.AddValue ("writeFlowMonitor", "<0/1> to enable Flow Monitor and write their results", flowMonitor);
  cmd.AddValue ("jsonFile", "Path to the desired bmv2 JSON file", jsonFile);
  cmd.AddValue ("commandsFile", "Path to the desired bmv2 CLI commands file", commandsFile);

  cmd.Parse (argc, argv);
  if ( (testNum != 1) && (testNum != 3) && (testNum != 4) )
    {
      NS_ABORT_MSG ("Invalid test number. Supported tests are 1, 3, 4");
    }

  NS_LOG_INFO ("Create nodes");
  NodeContainer c;
  c.Create (6);
  Names::Add ( "N0", c.Get (0));
  Names::Add ( "N1", c.Get (1));
  Names::Add ( "N2", c.Get (2));
  Names::Add ( "N3", c.Get (3));
  Names::Add ( "N4", c.Get (4));
  Names::Add ( "N5", c.Get (5));
  n0n2 = NodeContainer (c.Get (0), c.Get (2));
  n1n2 = NodeContainer (c.Get (1), c.Get (2));
  n2n3 = NodeContainer (c.Get (2), c.Get (3));
  n3n4 = NodeContainer (c.Get (3), c.Get (4));
  n3n5 = NodeContainer (c.Get (3), c.Get (5));

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  // 42 = headers size
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1000 - 42));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  TrafficControlHelper tchQdisc;
  configQdisc(qdiscSelection, tchQdisc);

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (c);

  TrafficControlHelper tchPfifo;
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue ("1000p"));

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devn0n2 = p2p.Install (n0n2);
  tchPfifo.Install (devn0n2);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer devn1n2 = p2p.Install (n1n2);
  tchPfifo.Install (devn1n2);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue (bnLinkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (bnLinkDelay));
  NetDeviceContainer devn2n3 = p2p.Install (n2n3);
  // only backbone link has selected queue disc implemenation
  QueueDiscContainer queueDiscs = tchQdisc.Install (devn2n3);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("4ms"));
  NetDeviceContainer devn3n4 = p2p.Install (n3n4);
  tchPfifo.Install (devn3n4);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer devn3n5 = p2p.Install (n3n5);
  tchPfifo.Install (devn3n5);

  NS_LOG_INFO ("Assign IP Addresses");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  i0i2 = ipv4.Assign (devn0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  i1i2 = ipv4.Assign (devn1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  i2i3 = ipv4.Assign (devn2n3);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  i3i4 = ipv4.Assign (devn3n4);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  i3i5 = ipv4.Assign (devn3n5);

  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  BuildAppsTest ();

  if (writePcap)
    {
      PointToPointHelper ptp;
      std::stringstream stmp;
      stmp << pathOut << "/" << qdiscSelection;
      ptp.EnablePcapAll (stmp.str ().c_str ());
    }

  Ptr<FlowMonitor> flowmon;
  if (flowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

  if (writeForPlot)
    {
      filePlotQueue << pathOut << "/" << qdiscSelection << "/" << qdiscSelection << "-queue.plotme";
      filePlotQueueAvg << pathOut << "/" << qdiscSelection << "/" << qdiscSelection << "-queue_avg.plotme";

      remove (filePlotQueue.str ().c_str ());
      remove (filePlotQueueAvg.str ().c_str ());
      Ptr<QueueDisc> queue = queueDiscs.Get (0);
      Simulator::ScheduleNow (&CheckQueueSize, queue);
    }

  Simulator::Stop (Seconds (sink_stop_time));
  Simulator::Run ();

  if (flowMonitor)
    {
      std::stringstream stmp;
      stmp << pathOut << "/" << qdiscSelection << "/" << qdiscSelection << ".flowmon";

      flowmon->SerializeToXmlFile (stmp.str ().c_str (), false, false);
    }

  if (printStats)
    {
      QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();
      std::cout << "*** " << qdiscSelection << " stats from Node 2 queue disc ***" << std::endl;
      std::cout << st << std::endl;

      st = queueDiscs.Get (1)->GetStats ();
      std::cout << "*** " << qdiscSelection << " stats from Node 3 queue disc ***" << std::endl;
      std::cout << st << std::endl;
    }

  Simulator::Destroy ();

  return 0;
}

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
 */

// Network topology
//
//             n0
//             |
//       --------------
//       |  (router)  |
//       |            |
//       | [p4-qdisc] |
//       --------------
//             | 
//             n1
//
//
// - CBR/UDP flow from n0 to n1
// - P4 qdisc at egress link of router 
// - Tracing of queues and packet receptions to file "router.tr"

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleP4QdiscExample");

void
TcBytesInQueueTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newValue << std::endl;
}

void
TcDropTrace (Ptr<const QueueDiscItem> item)
{
  std::cout << "TC dropped packet!" << std::endl;
}

void
DeviceBytesInQueueTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newValue << std::endl;
}

void
DeviceDropTrace (Ptr<const Packet> p)
{
  std::cout << "Device dropped packet!" << std::endl;
}

int 
main (int argc, char *argv[])
{
  //
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //
  LogComponentEnable ("SimpleP4QdiscExample", LOG_LEVEL_INFO);

  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.Parse (argc, argv);

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  NS_LOG_INFO ("Create nodes.");
  Ptr<Node> n0 = CreateObject<Node> ();
  Ptr<Node> n1 = CreateObject<Node> ();
  Ptr<Node> router = CreateObject<Node> ();

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  // Create the csma links, from each terminal to the router
  NetDeviceContainer n0rDevices = csma.Install (NodeContainer (n0, router));
  NetDeviceContainer n1rDevices = csma.Install (NodeContainer (n1, router));

  Ptr<NetDevice> n1Device = n1rDevices.Get (0);
  Ptr<NetDevice> rDevice = n1rDevices.Get (1);

  // Add internet stack to the all nodes 
  InternetStackHelper internet;
  internet.Install (NodeContainer (n0, n1, router));

  TrafficControlHelper tch;
  //tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tch.SetRootQueueDisc ("ns3::P4QueueDisc",
                        "JsonFile", StringValue("src/traffic-control/examples/p4-src/basic-test.json"),
                        "CommandsFile", StringValue("src/traffic-control/examples/p4-src/commands.txt")
                        );

  // Install Queue Disc on the router interface towards n2
  QueueDiscContainer qdiscs = tch.Install (rDevice);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (n0rDevices);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  ipv4.Assign (n1rDevices);

  // Initialize routing database and set up the routing tables in the nodes. 
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //
  // Start the client on n0
  //
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)

  Address n1Address (InetSocketAddress (Ipv4Address ("10.1.2.1"), port));

  OnOffHelper onoff ("ns3::UdpSocketFactory", n1Address);
  onoff.SetConstantRate (DataRate ("3Mbps"));
  onoff.SetAttribute ("MaxBytes", UintegerValue (1000));

  // Start the application on n0
  ApplicationContainer app = onoff.Install (n0);
  app.Start (Seconds (1.0));
  app.Stop (Seconds (5.0));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory", n1Address);
  app = sink.Install (n1);
  app.Start (Seconds (0.0));

  NS_LOG_INFO ("Configure Tracing.");
  //
  // Configure tracing of both TC queue and NetDevice Queue at bottleneck 
  //
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> tcStream = asciiTraceHelper.CreateFileStream ("trace-data/tc-qsize.txt");
  Ptr<QueueDisc> qdisc = qdiscs.Get (0);
  qdisc->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&TcBytesInQueueTrace, tcStream));
  qdisc->TraceConnectWithoutContext ("Drop", MakeCallback (&TcDropTrace));

  Ptr<OutputStreamWrapper> devStream = asciiTraceHelper.CreateFileStream ("trace-data/dev-qsize.txt");
  Ptr<CsmaNetDevice> csmaNetDev = DynamicCast<CsmaNetDevice> (rDevice);
  Ptr<Queue<Packet>> queue = csmaNetDev->GetQueue ();
  queue->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&DeviceBytesInQueueTrace, devStream));
  queue->TraceConnectWithoutContext ("Drop", MakeCallback (&DeviceDropTrace));

  //
  // Setup pcap capture on n1's NetDevice.
  // Can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcap ("trace-data/remote", n1Device);

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

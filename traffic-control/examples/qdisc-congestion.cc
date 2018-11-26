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
//        n0     n1
//        |      | 
//       ----------
//       | router |
//       ----------
//           | 
//           n2
//
//
// - CBR/UDP flows from n0 and n1 to n2
// - DropTail queues 
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
#include "ns3/packet-filter.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RouterCongestion");

//-------------------------------------------------------//
//------------- PifoQueueDiscTestFilter -----------------//
//-------------------------------------------------------//

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Queue Disc Test Packet Filter
 */
class PifoQueueDiscTestFilter : public PacketFilter
{
public:
  /**
   * Constructor
   *
   * \param cls whether this filter is able to classify a PifoQueueDiscTestItem
   */
  PifoQueueDiscTestFilter (bool cls);
  virtual ~PifoQueueDiscTestFilter ();
  /**
   * \brief Set the value returned by DoClassify
   *
   * \param ret the value that DoClassify returns
   */
  void SetReturnValue (int32_t ret);

private:
  virtual bool CheckProtocol (Ptr<QueueDiscItem> item) const;
  virtual int32_t DoClassify (Ptr<QueueDiscItem> item) const;

  bool m_cls;     //!< whether this filter is able to classify a PifoQueueDiscTestItem
  int32_t m_ret;  //!< the value that DoClassify returns if m_cls is true
};

PifoQueueDiscTestFilter::PifoQueueDiscTestFilter (bool cls)
  : m_cls (cls),
    m_ret (0)
{
}

PifoQueueDiscTestFilter::~PifoQueueDiscTestFilter ()
{
}

void
PifoQueueDiscTestFilter::SetReturnValue (int32_t ret)
{
  m_ret = ret;
}

bool
PifoQueueDiscTestFilter::CheckProtocol (Ptr<QueueDiscItem> item) const
{
  return m_cls;
}

int32_t
PifoQueueDiscTestFilter::DoClassify (Ptr<QueueDiscItem> item) const
{
  return m_ret;
}

//-------------------------------------------------//
//------------- trace callbacks -------------------//
//-------------------------------------------------//
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
  LogComponentEnable ("RouterCongestion", LOG_LEVEL_INFO);

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
  Ptr<Node> n2 = CreateObject<Node> ();
  Ptr<Node> router = CreateObject<Node> ();

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  // Create the csma links, from each terminal to the router
  NetDeviceContainer n0rDevices = csma.Install (NodeContainer (n0, router));
  NetDeviceContainer n1rDevices = csma.Install (NodeContainer (n1, router));
  NetDeviceContainer n2rDevices = csma.Install (NodeContainer (n2, router));

  Ptr<NetDevice> rDevice = n2rDevices.Get (1);
  Ptr<NetDevice> n2Device = n2rDevices.Get (0);

  // Add internet stack to the all nodes 
  InternetStackHelper internet;
  internet.Install (NodeContainer (n0, n1, n2, router));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PifoQueueDisc");
//  uint16_t handle = tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
//  tch.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue ("1000p"));

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

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  ipv4.Assign (n2rDevices);

  // Initialize routing database and set up the routing tables in the nodes. 
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //
  // Start the clients on n0 and n1
  //
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)

  Address n2Address (InetSocketAddress (Ipv4Address ("10.1.3.1"), port));

  OnOffHelper onoff ("ns3::UdpSocketFactory", n2Address);
  onoff.SetConstantRate (DataRate ("3Mbps"));
//  onoff.SetAttribute ("MaxBytes", UintegerValue (1500*10));

  // Start the application on n0
  ApplicationContainer app = onoff.Install (n0);
  app.Start (Seconds (1.0));
  app.Stop (Seconds (5.0));

  // Start the application on n1
  app = onoff.Install (n1);
  app.Start (Seconds (1.5));
  app.Stop (Seconds (5.0));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory", n2Address);
  app = sink.Install (n2);
  app.Start (Seconds (0.0));

  NS_LOG_INFO ("Configure Tracing.");
  //
  // Configure tracing of both TC queue and NetDevice Queue at bottleneck 
  //
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> tcStream = asciiTraceHelper.CreateFileStream ("trace-data/tc-qsize.txt");
  Ptr<QueueDisc> qdisc = qdiscs.Get (0);
  qdisc->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&TcBytesInQueueTrace, tcStream));
//  qdisc->TraceConnectWithoutContext ("Drop", MakeCallback (&TcDropTrace));

  Ptr<OutputStreamWrapper> devStream = asciiTraceHelper.CreateFileStream ("trace-data/dev-qsize.txt");
  Ptr<CsmaNetDevice> csmaNetDev = DynamicCast<CsmaNetDevice> (rDevice);
  Ptr<Queue<Packet>> queue = csmaNetDev->GetQueue ();
  queue->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&DeviceBytesInQueueTrace, devStream));
  queue->TraceConnectWithoutContext ("Drop", MakeCallback (&DeviceDropTrace));

  //
  // Setup pcap capture on n2's NetDevice.
  // Can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcap ("trace-data/remote", n2Device);

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

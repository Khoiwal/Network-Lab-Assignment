/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008-2009 Strasbourg University
 * Copyright (c) 2013 Universita' di Firenze
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
 * Author: David Gross <gdavid.devel@gmail.com>
 *         Sebastien Vincent <vincent@clarinet.u-strasbg.fr>
 * Modified by Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

// Network topology
// //
// //     Src     n0   r    n1    Dst
// //             |    _    |
// //     MTU     ====|_|====     MTU
// //     5000       router       1500
// //
// // - Tracing of queues and packet receptions to file "fragmentation-ipv6-two-mtu.tr"

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

#include "ns3/mobility-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"
#include "myapp.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6TwoMtuExample");

int main (int argc, char** argv)
{

  bool verbose = false;
  //LogComponentEnable ("Packet", LOG_LEVEL_FUNCTION);
  CommandLine cmd;
  cmd.AddValue ("verbose", "turn on log components", verbose);
  cmd.Parse (argc, argv);
  //std :: cout << "Hello world";
  if (verbose)
    {
      LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv6StaticRouting", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv6Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
    }

  NS_LOG_INFO ("Create nodes.");
  Ptr<Node> n0 = CreateObject<Node> ();
  Ptr<Node> r = CreateObject<Node> ();
  Ptr<Node> n1 = CreateObject<Node> ();

  NodeContainer net1 (n0, r);
  NodeContainer net2 (r, n1);
  NodeContainer all (n0, r, n1);

  NS_LOG_INFO ("Create IPv6 Internet Stack");
  InternetStackHelper internetv6;
  internetv6.Install (all);

  Ptr<Ipv6> ipv6n0 = n0->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6r = r->GetObject<Ipv6> ();

  NS_LOG_INFO ("Create channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d2 = csma.Install (net2);
  csma.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer d1 = csma.Install (net1);

  NS_LOG_INFO ("Create networks and assign IPv6 Addresses.");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1 = ipv6.Assign (d1);
  i1.SetForwarding (1, true);
  i1.SetDefaultRouteInAllNodes (1);
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i2 = ipv6.Assign (d2);
  i2.SetForwarding (0, true);
  i2.SetDefaultRouteInAllNodes (0);



  // TCP connection from N0 to N2

  uint16_t sinkPort = 6;
  Address sinkAddress (Inet6SocketAddress (i2.GetAddress (1,1), sinkPort)); // interface of n2
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", Inet6SocketAddress (Ipv6Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (net2.Get (1)); //n2 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (100.));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (net1.Get (0), TcpSocketFactory::GetTypeId ()); //source at n0

  // Create TCP application at n0
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 4096, 1, DataRate ("250Kbps"));
  net1.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (37.));
  app->SetStopTime (Seconds (100.));


  NS_LOG_INFO ("Create Applications.");


  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("major-project.tr"));
  csma.EnablePcapAll (std::string ("major-project"), true);

  NS_LOG_INFO ("Run Simulation.");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
  monitor = flowmon.InstallAll();
  monitor->CheckForLostPackets ();


  Simulator::Stop (Seconds (200.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier> (flowmon.GetClassifier6 ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
	  Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
        {
    	  std :: cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress;
    	  std :: cout << "Tx Packets = " << iter->second.txPackets;
    	  std :: cout << "Rx Packets = " << iter->second.rxPackets;
    	  std :: cout << "Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024  << " Kbps";
        }
    }
  monitor->SerializeToXmlFile("lab-5.flowmon", true, true);
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  //Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  //std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;

}


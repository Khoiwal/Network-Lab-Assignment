/* DSR ROUTING */

#include "ns3/dsr-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/position-allocator.h"
#include "ns3/gnuplot.h"

#include <sstream>
#include <string>
#include <iostream>
#include <cassert>
#include <fstream>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Dsr-Routing");

void
ReceivePacket(Ptr<const Packet> p, const Address & addr)
{
	std::cout << Simulator::Now ().GetSeconds () << "\t" << p->GetSize() <<"\n";
}

uint32_t MacTxDropCount, PhyTxDropCount, PhyRxDropCount;

void
MacTxDrop(Ptr<const Packet> p)
{
  NS_LOG_INFO("Packet Drop");
  MacTxDropCount++;
}


void
PhyTxDrop(Ptr<const Packet> p)
{
  NS_LOG_INFO("Packet Drop");
  PhyTxDropCount++;
}
void
PhyRxDrop(Ptr<const Packet> p)
{
  NS_LOG_INFO("Packet Drop");
  PhyRxDropCount++;
}

void
PrintDrop()
{
  std::cout << Simulator::Now().GetSeconds() << "\t" << MacTxDropCount << "\t"<< PhyTxDropCount << "\t" << PhyRxDropCount << "\n";
  Simulator::Schedule(Seconds(5.0), &PrintDrop);
}

// for selected modules; the below lines suggest how to do this
int
main (int argc, char *argv[])
{

  NS_LOG_INFO ("creating the nodes");

  // General parameters
  uint32_t nWifis = 50;
  uint32_t nSinks = 25;
  double TotalTime = 30.0;
  double dataTime = 20.0;
  double dataStart = 10.0; // start sending data at 100s
  
  //double pauseTime = 0.0;
  //double nodeSpeed = 20.0;
  //double txpDistance = 250.0;

  bool enableFlowMonitor = false;
  std::string rate = "0.512kbps";
  std::string phyMode ("DsssRate11Mbps");
 //std::string dataMode ("DsssRate11Mbps");

  //Allow users to override the default parameters and set it to new ones from CommandLine.
  CommandLine cmd;
  cmd.AddValue ("nWifis", "Number of wifi nodes", nWifis);
  cmd.AddValue ("nSinks", "Number of SINK traffic nodes", nSinks);
  //cmd.AddValue ("txpDistance", "Specify node's transmit range, Default:300", txpDistance);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);


  //SeedManager::SetSeed (10);
  //SeedManager::SetRun (1);

  NodeContainer adhocNodes;
  adhocNodes.Create (nWifis);
  NetDeviceContainer allDevices;

  NS_LOG_INFO ("setting the default phy and channel parameters");
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  NS_LOG_INFO ("setting the default phy and channel parameters ");

//VANET 802.11b 2.4 GHz
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	  	  	  	  	  	  	  	    "SystemLoss", DoubleValue(1),
  	  	  	  	  	                            "HeightAboveZ", DoubleValue(1.5));

  // For range
  wifiPhy.Set ("TxPowerStart", DoubleValue(33));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(33));
  wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set ("TxGain", DoubleValue(0));
  wifiPhy.Set ("RxGain", DoubleValue(0));  
  wifiPhy.SetChannel (wifiChannel.Create ());


// Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode",
                                StringValue (phyMode));
  wifiMac.SetType ("ns3::AdhocWifiMac");
  allDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);

/// Vanet_End ///


//Set Up Constant Mobility
  NS_LOG_INFO ("Configure Tracing.");
  MobilityHelper adhocMobility;
  int64_t streamIndex = 0; // used to get consistent mobility across scenarios
  
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  adhocMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  adhocMobility.SetPositionAllocator (taPositionAlloc);
  
  streamIndex += adhocMobility.AssignStreams (adhocNodes, streamIndex);
  //std::stringstream ssSpeed;
  //ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
  //std::stringstream ssPause;
  //ssPause << "ns3::ConstantRandomVariable[Constant=" << pauseTime << "]";
  
  adhocMobility.Install (adhocNodes);


  // *****Set up internet stack
  InternetStackHelper internet;
  DsrMainHelper dsrMain;
  DsrHelper dsr;
  internet.Install (adhocNodes);
  dsrMain.Install (dsr, adhocNodes);

  // *****Set up Addresses
  NS_LOG_INFO ("assigning ip address");
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer allInterfaces;
  allInterfaces = address.Assign (allDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  double randomStartTime = (1 / nSinks); //distributed btw 1s evenly as we are sending 4pkt/s

  for (uint32_t i = 0; i < nSinks; ++i)
    {
      PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer apps_sink = sink.Install (adhocNodes.Get (i));
      apps_sink.Start (Seconds (0.0));
      apps_sink.Stop (Seconds (TotalTime));

      OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (allInterfaces.GetAddress (i), port)));
      onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
      onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
      onoff1.SetAttribute ("PacketSize", StringValue ("512Kbps"));
      onoff1.SetAttribute ("DataRate", DataRateValue (DataRate (rate)));

      ApplicationContainer apps1 = onoff1.Install (adhocNodes.Get (i + nWifis - nSinks));
      apps1.Start (Seconds (dataStart + i * randomStartTime));
      apps1.Stop (Seconds (dataTime + i * randomStartTime));
    }

// Users may find it convenient to turn on explicit debugging
  //LogComponentEnable ("DsrOptions", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrHelper", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrRouting", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrOptionHeader", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrFsHeader", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrGraReplyTable", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrSendBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable ("RouteCache", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrMaintainBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable ("RreqTable", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrErrorBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable ("DsrNetworkQueue", LOG_LEVEL_ALL);

//NetAnim Config
      AnimationInterface anim ("DSR-ROUTING.xml"); 
      anim.EnablePacketMetadata(true);


//*****Print Out Ascii

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("DSR-ROUTING.tr");
  wifiPhy.EnableAsciiAll (stream);

//*****Calculate Throughput using Flowmonitor

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&ReceivePacket));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&PhyTxDrop));


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Schedule(Seconds(0.0),&PrintDrop);
  Simulator::Stop (Seconds(TotalTime));
  Simulator::Run ();
  NS_LOG_UNCOND ("Flow Monitor Statistic");

// *****GNU PLOT PARAMETER
   std::string graphicsFileName = "DSR-ROUTING.png";
   std::string plotFileName = "DSR-ROUTING.plt";
//
  Gnuplot gnuplot (graphicsFileName);
  Gnuplot2dDataset dataset;

  //Gnuplot ...continued
  gnuplot.AddDataset (dataset);
  // Open the plot file.
  std::ofstream plotFile (plotFileName.c_str());
  // Write the plot file.
  gnuplot.GenerateOutput (plotFile);
  // Close the plot file.
  plotFile.close ();



//.................
  PrintDrop();
 monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

        uint32_t txPacketsum = 0;
        uint32_t rxPacketsum = 0;
        uint32_t DropPacketsum = 0;
        uint32_t LostPacketsum = 0;
        uint32_t rxBytessum = 0;
        double Delaysum = 0; 
	
	

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
{
  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
                txPacketsum += i->second.txPackets;
                rxPacketsum += i->second.rxPackets;
                LostPacketsum += i->second.lostPackets;
                DropPacketsum += i->second.packetsDropped.size();
                Delaysum += i->second.delaySum.GetSeconds();
                rxBytessum += i->second.rxBytes;

  

// Parameters NS_LOG for Throughput , dll
          
          NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress );
    	  NS_LOG_UNCOND("Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds()-i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " Kbps");  
          NS_LOG_UNCOND("Tx Packets: " << i->second.txPackets);
          NS_LOG_UNCOND("Rx Packets: " << i->second.rxPackets);
          NS_LOG_UNCOND("Delay: " << i->second.delaySum.GetSeconds());
          NS_LOG_UNCOND("Lost Packets: " << i->second.lostPackets);
          NS_LOG_UNCOND("Drop Packets: " << i->second.packetsDropped.size()); 
          NS_LOG_UNCOND("Packets Delivery Ratio: " << ((i->second.rxPackets * 100) / i->second.txPackets) << "%");
          NS_LOG_UNCOND("Packets Lost Ratio: " << ((i->second.lostPackets * 100) / i->second.txPackets) << "%" << "\n");
        dataset.Add((double)Simulator::Now().GetSeconds(),(double) rxPacketsum);

 	}

  monitor->SerializeToXmlFile("DSR-ROUTING.xml", true, true);
  //NS_LOG_UNCOND("Average PLR: " << ((LostPacketsum * 100) / txPacketsum) << " %");  
  NS_LOG_UNCOND("Average PDR: " << ((rxPacketsum * 100) / txPacketsum) << " %"); 
  NS_LOG_UNCOND("Average Throughput: " << ((rxBytessum * 8.0) / (TotalTime - 20.0)) / 1024 / nSinks  << " Kbps");
  NS_LOG_UNCOND("Average Delay: " << (Delaysum / rxPacketsum) * 1000 << " ms" << "\n");	

Simulator::Destroy ();	

}

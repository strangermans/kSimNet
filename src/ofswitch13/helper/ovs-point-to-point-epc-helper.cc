/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2013 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Jaume Nin <jnin@cttc.es>
 *         Nicola Baldo <nbaldo@cttc.es>
 *         Manuel Requena <manuel.requena@cttc.es>
 */

#include <ns3/point-to-point-epc-helper.h>
#include <ns3/log.h>
#include <ns3/inet-socket-address.h>
#include <ns3/mac48-address.h>
#include <ns3/eps-bearer.h>
#include <ns3/ipv4-address.h>
#include <ns3/internet-stack-helper.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/packet-socket-helper.h>
#include <ns3/packet-socket-address.h>
#include <ns3/epc-enb-application.h>
#include <ns3/epc-sgw-pgw-application.h>

#include <ns3/lte-enb-rrc.h>
#include <ns3/epc-x2.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-ue-net-device.h>
#include <ns3/epc-mme.h>
#include <ns3/epc-ue-nas.h>

#include <ns3/ovs-point-to-point-epc-helper.h>

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>

#include <ns3/qos-controller.h> // dhson

//#include "../examples/ofswitch13-qos-controller/qos-controller.h"
//#include "qos-controller.h"

#include "iostream"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OvsPointToPointEpcHelper");

NS_OBJECT_ENSURE_REGISTERED (OvsPointToPointEpcHelper);

int cnt_tmp=0;

OvsPointToPointEpcHelper::OvsPointToPointEpcHelper () 
  : m_gtpuUdpPort (2152)  // fixed by the standard
{
  NS_LOG_FUNCTION (this);

  // since we use point-to-point links for all S1-U links, 
  // we use a /30 subnet which can hold exactly two addresses 
  // (remember that net broadcast and null address are not valid)
  m_s1uIpv4AddressHelper.SetBase ("10.1.0.0", "255.255.0.0", "0.0.1.2");
  //cout<<"OvsP2PH activate"<<endl;
  //m_s1uIpv4AddressHelper.SetBase ("10.1.1.0", "255.255.255.0");

  m_x2Ipv4AddressHelper.SetBase ("12.0.0.0", "255.255.255.252");

  // we use a /8 net for all UEs
  m_ueAddressHelper.SetBase ("7.0.0.0", "255.0.0.0");
  
  // create SgwPgwNode
  m_sgwPgw = CreateObject<Node> ();
  InternetStackHelper internet;
  internet.Install (m_sgwPgw);
  
  // create S1-U socket
  Ptr<Socket> sgwPgwS1uSocket = Socket::CreateSocket (m_sgwPgw, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = sgwPgwS1uSocket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_gtpuUdpPort));
  NS_ASSERT (retval == 0);

  // create TUN device implementing tunneling of user data over GTP-U/UDP/IP 
  m_tunDevice = CreateObject<VirtualNetDevice> ();
  // allow jumbo packets
  m_tunDevice->SetAttribute ("Mtu", UintegerValue (30000));

  // yes we need this
  m_tunDevice->SetAddress (Mac48Address::Allocate ()); 

  m_sgwPgw->AddDevice (m_tunDevice);
  NetDeviceContainer tunDeviceContainer;
  tunDeviceContainer.Add (m_tunDevice);
  
  // the TUN device is on the same subnet as the UEs, so when a packet
  // addressed to an UE arrives at the intenet to the WAN interface of
  // the PGW it will be forwarded to the TUN device. 
  Ipv4InterfaceContainer tunDeviceIpv4IfContainer = m_ueAddressHelper.Assign (tunDeviceContainer);  

  // create EpcSgwPgwApplication
  m_sgwPgwApp = CreateObject<EpcSgwPgwApplication> (m_tunDevice, sgwPgwS1uSocket);
  m_sgwPgw->AddApplication (m_sgwPgwApp);
  
  // connect SgwPgwApplication and virtual net device for tunneling
  m_tunDevice->SetSendCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromTunDevice, m_sgwPgwApp));

  // Create MME and connect with SGW via S11 interface
  m_mme = CreateObject<EpcMme> ();
  m_mme->SetS11SapSgw (m_sgwPgwApp->GetS11SapSgw ());
  m_sgwPgwApp->SetS11SapMme (m_mme->GetS11SapMme ());
}

OvsPointToPointEpcHelper::~OvsPointToPointEpcHelper ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
OvsPointToPointEpcHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OvsPointToPointEpcHelper")
    .SetParent<EpcHelper> ()
    .SetGroupName("Lte")
    .AddConstructor<OvsPointToPointEpcHelper> ()
    .AddAttribute ("S1uLinkDataRate", 
                   "The data rate to be used for the next S1-U link to be created",
                   DataRateValue (DataRate ("10Gb/s")),
                   MakeDataRateAccessor (&OvsPointToPointEpcHelper::m_s1uLinkDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("S1uLinkDelay", 
                   "The delay to be used for the next S1-U link to be created",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&OvsPointToPointEpcHelper::m_s1uLinkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("S1uLinkMtu", 
                   "The MTU of the next S1-U link to be created. Note that, because of the additional GTP/UDP/IP tunneling overhead, you need a MTU larger than the end-to-end MTU that you want to support.",
                   UintegerValue (2000),
                   MakeUintegerAccessor (&OvsPointToPointEpcHelper::m_s1uLinkMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("X2LinkDataRate",
                   "The data rate to be used for the next X2 link to be created",
                   DataRateValue (DataRate ("10Gb/s")),
                   MakeDataRateAccessor (&OvsPointToPointEpcHelper::m_x2LinkDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("X2LinkDelay",
                   "The delay to be used for the next X2 link to be created",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&OvsPointToPointEpcHelper::m_x2LinkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("X2LinkMtu",
                   "The MTU of the next X2 link to be created. Note that, because of some big X2 messages, you need a big MTU.",
                   UintegerValue (3000),
                   MakeUintegerAccessor (&OvsPointToPointEpcHelper::m_x2LinkMtu),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

void
OvsPointToPointEpcHelper::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_tunDevice->SetSendCallback (MakeNullCallback<bool, Ptr<Packet>, const Address&, const Address&, uint16_t> ());
  m_tunDevice = 0;
  m_sgwPgwApp = 0;  
  m_sgwPgw->Dispose ();
}


//Input: enb(eNodeB), lteEnbNetDevice(Netdevice for eNodeB), cellId
//Output: void
//Explanation: This function places 3 SDN switches and 2 SDN controllers between eNodeB and sgw/pgw
void
OvsPointToPointEpcHelper::AddEnb (Ptr<Node> enb, Ptr<NetDevice> lteEnbNetDevice, uint16_t cellId)
{
  NS_LOG_FUNCTION (this << enb << lteEnbNetDevice << cellId);

  NS_ASSERT (enb == lteEnbNetDevice->GetNode ());

  // add an IPv4 stack to the previously created eNB
  InternetStackHelper internet;
  internet.Install (enb);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after node creation: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());

/*
 // create a point to point link between the new eNB and the SGW with
 // the corresponding new NetDevices on each side  

  NodeContainer enbSgwNodes;
  enbSgwNodes.Add (m_sgwPgw);
  enbSgwNodes.Add (enb);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (m_s1uLinkDataRate));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (m_s1uLinkMtu));
  p2ph.SetChannelAttribute ("Delay", TimeValue (m_s1uLinkDelay));
  NetDeviceContainer enbSgwDevices = p2ph.Install (enb, m_sgwPgw);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after installing p2p dev: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());
  Ptr<NetDevice> enbDev = enbSgwDevices.Get (0);
  Ptr<NetDevice> sgwDev = enbSgwDevices.Get (1);
  m_s1uIpv4AddressHelper.NewNetwork ();
  Ipv4InterfaceContainer enbSgwIpIfaces = m_s1uIpv4AddressHelper.Assign (enbSgwDevices);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after assigning Ipv4 addr to S1 dev: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());


  Ipv4Address enbAddress = enbSgwIpIfaces.GetAddress (0);
  Ipv4Address sgwAddress = enbSgwIpIfaces.GetAddress (1);
*/
  NodeContainer switchNodes, controllerNodes;
  switchNodes.Create(3);
  controllerNodes.Create(2);
  
  NetDeviceContainer serverDevices, clientDevices;
  NetDeviceContainer switch0Ports, switch1Ports, switch2Ports;
  NetDeviceContainer link;

  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get(0), switchNodes.Get(1)));
  switch0Ports.Add(link.Get(0));
  switch1Ports.Add(link.Get(1));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get (0), switchNodes.Get (1)));
  switch0Ports.Add (link.Get (0));
  switch1Ports.Add (link.Get (1));

  // Configure the CsmaHelper for 100Mbps connections
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));

  // Connect aggregation switch to client switch
  link = csmaHelper.Install (NodeContainer (switchNodes.Get (1), switchNodes.Get (2)));
  switch1Ports.Add (link.Get (0));
  switch2Ports.Add (link.Get (1));

  // Connect servers to border switch
  link = csmaHelper.Install (NodeContainer (m_sgwPgw, switchNodes.Get (0)));
  serverDevices.Add (link.Get (0));
  switch0Ports.Add (link.Get (1));

  // connect clients to aggregate switch
  link = csmaHelper.Install (NodeContainer (enb, switchNodes.Get (2)));
  clientDevices.Add (link.Get (0));
  switch2Ports.Add (link.Get (1));
  
  // Configure OpenFlow QoS controller for border and aggregation switches
  // (#0 and #1) into controller node 0.
  Ptr<OFSwitch13InternalHelper> ofQosHelper =
    CreateObject<OFSwitch13InternalHelper> ();
  Ptr<QosController> qosCtrl = CreateObject<QosController> ();
  ofQosHelper->InstallController (controllerNodes.Get (0), qosCtrl);

  // Configure OpenFlow learning controller for client switch (#2) into controller node 1
  Ptr<OFSwitch13InternalHelper> ofLearningHelper = CreateObject<OFSwitch13InternalHelper> ();
  Ptr<OFSwitch13LearningController> learnCtrl = CreateObject<OFSwitch13LearningController> ();
  ofLearningHelper->InstallController (controllerNodes.Get (1), learnCtrl);

  // Install OpenFlow switches 0 and 1 with border controller
  OFSwitch13DeviceContainer ofSwitchDevices;
  ofSwitchDevices.Add (ofQosHelper->InstallSwitch (switchNodes.Get (0), switch0Ports));
  ofSwitchDevices.Add (ofQosHelper->InstallSwitch (switchNodes.Get (1), switch1Ports));
  ofQosHelper->CreateOpenFlowChannels ();

  // Install OpenFlow switches 2 with learning controller
  ofSwitchDevices.Add (ofLearningHelper->InstallSwitch (switchNodes.Get (2), switch2Ports));
  ofLearningHelper->CreateOpenFlowChannels ();
 
  Ipv4AddressHelper ipv4switches;
  Ipv4InterfaceContainer internetIpIfaces;
  ipv4switches.SetBase ("10.1.0.0", "255.255.0.0", "0.0.1.2");
  internetIpIfaces = ipv4switches.Assign (serverDevices);
  Ipv4Address sgwAddress = internetIpIfaces.GetAddress(0);
  ipv4switches.SetBase ("10.1.0.0", "255.255.0.0", "0.0.2.1");
  internetIpIfaces = ipv4switches.Assign (clientDevices);
  Ipv4Address enbAddress = internetIpIfaces.GetAddress(0);

  Ipv4Address serverAddr ("10.1.1.1");

  Ptr<NetDevice> enbDev = clientDevices.Get(0);
  Ptr<NetDevice> sgwDev = serverDevices.Get(0); 


  //cout<<"enbAddress: "<<enbAddress<<endl;
  //cout<<"sgwAddress: "<<sgwAddress<<endl;
/*
  cout<<"---border switch---"<<endl;
  for(uint32_t i=0; i<switch0Ports.GetN(); i++)
  {
    //cout<<"port"<<i<<": "<<switch0Ports.Get(i)->GetAddress()<<endl;
  } 
 
  cout<<"---aggregation switch---"<<endl;
  for(uint32_t i=0; i<switch1Ports.GetN(); i++)
  {
    cout<<"port"<<i<<": "<<switch1Ports.Get(i)->GetAddress()<<endl;
  } 
 

 
  cout<<"---client switch---"<<endl;
   for(uint32_t i=0; i<switch2Ports.GetN(); i++)
  {
    cout<<"port"<<i<<": "<<switch2Ports.Get(i)->GetAddress()<<endl;
  } 
  
*/

  // create S1-U socket for the ENB
  Ptr<Socket> enbS1uSocket = Socket::CreateSocket (enb, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = enbS1uSocket->Bind (InetSocketAddress (enbAddress, m_gtpuUdpPort));
  NS_ASSERT (retval == 0);


  // give PacketSocket powers to the eNB
  //PacketSocketHelper packetSocket;
  //packetSocket.Install (enb); 

  // create LTE socket for the ENB 
  Ptr<Socket> enbLteSocket = Socket::CreateSocket (enb, TypeId::LookupByName ("ns3::PacketSocketFactory"));
  PacketSocketAddress enbLteSocketBindAddress;
  enbLteSocketBindAddress.SetSingleDevice (lteEnbNetDevice->GetIfIndex ());
  enbLteSocketBindAddress.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
  retval = enbLteSocket->Bind (enbLteSocketBindAddress);
  NS_ASSERT (retval == 0);
  PacketSocketAddress enbLteSocketConnectAddress;
  enbLteSocketConnectAddress.SetPhysicalAddress (Mac48Address::GetBroadcast ());
  enbLteSocketConnectAddress.SetSingleDevice (lteEnbNetDevice->GetIfIndex ());
  enbLteSocketConnectAddress.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
  retval = enbLteSocket->Connect (enbLteSocketConnectAddress);
  NS_ASSERT (retval == 0);


  NS_LOG_INFO ("create EpcEnbApplication");
 
  Ptr<EpcEnbApplication> enbApp = CreateObject<EpcEnbApplication> (enbLteSocket, enbS1uSocket, enbAddress, sgwAddress, cellId);
  //Ptr<EpcEnbApplication> enbApp = CreateObject<EpcEnbApplication> (enbLteSocket, enbS1uSocket, enbAddress, serverAddr, cellId);
 
  enb->AddApplication (enbApp);
  NS_ASSERT (enb->GetNApplications () == 1);
  NS_ASSERT_MSG (enb->GetApplication (0)->GetObject<EpcEnbApplication> () != 0, "cannot retrieve EpcEnbApplication");
  NS_LOG_LOGIC ("enb: " << enb << ", enb->GetApplication (0): " << enb->GetApplication (0));


  NS_LOG_INFO ("Create EpcX2 entity");
  Ptr<EpcX2> x2 = CreateObject<EpcX2> ();
  enb->AggregateObject (x2);

  NS_LOG_INFO ("connect S1-AP interface");
  m_mme->AddEnb (cellId, enbAddress, enbApp->GetS1apSapEnb ());
//  m_sgwPgwApp->AddEnb (cellId, enbAddress, sgwAddress);
  m_sgwPgwApp->AddEnb (cellId, enbAddress, serverAddr);

  enbApp->SetS1apSapMme (m_mme->GetS1apSapMme ());

}

void
OvsPointToPointEpcHelper::AddEnbOvs (Ptr<Node> enb, Ptr<NetDevice> lteEnbNetDevice, uint16_t cellId, int n_sw)
{
  NS_LOG_FUNCTION (this << enb << lteEnbNetDevice << cellId);

  NS_ASSERT (enb == lteEnbNetDevice->GetNode ());

  // add an IPv4 stack to the previously created eNB
  InternetStackHelper internet;
  internet.Install (enb);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after node creation: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());

  // create a point to point link between the new eNB and the SGW with
  // the corresponding new NetDevices on each side  
  NodeContainer enbSgwNodes;
  enbSgwNodes.Add (m_sgwPgw);
  enbSgwNodes.Add (enb);
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (m_s1uLinkDataRate));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (m_s1uLinkMtu));
  p2ph.SetChannelAttribute ("Delay", TimeValue (m_s1uLinkDelay));  
  NetDeviceContainer enbSgwDevices = p2ph.Install (enb, m_sgwPgw);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after installing p2p dev: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());  
  Ptr<NetDevice> enbDev = enbSgwDevices.Get (0);
  Ptr<NetDevice> sgwDev = enbSgwDevices.Get (1);
  m_s1uIpv4AddressHelper.NewNetwork ();
  Ipv4InterfaceContainer enbSgwIpIfaces = m_s1uIpv4AddressHelper.Assign (enbSgwDevices);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after assigning Ipv4 addr to S1 dev: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());
  
  Ipv4Address enbAddress = enbSgwIpIfaces.GetAddress (0);
  Ipv4Address sgwAddress = enbSgwIpIfaces.GetAddress (1);

  // create S1-U socket for the ENB
  Ptr<Socket> enbS1uSocket = Socket::CreateSocket (enb, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = enbS1uSocket->Bind (InetSocketAddress (enbAddress, m_gtpuUdpPort));
  NS_ASSERT (retval == 0);
  

  // give PacketSocket powers to the eNB
  //PacketSocketHelper packetSocket;
  //packetSocket.Install (enb); 
  
  // create LTE socket for the ENB 
  Ptr<Socket> enbLteSocket = Socket::CreateSocket (enb, TypeId::LookupByName ("ns3::PacketSocketFactory"));
  PacketSocketAddress enbLteSocketBindAddress;
  enbLteSocketBindAddress.SetSingleDevice (lteEnbNetDevice->GetIfIndex ());
  enbLteSocketBindAddress.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
  retval = enbLteSocket->Bind (enbLteSocketBindAddress);
  NS_ASSERT (retval == 0);  
  PacketSocketAddress enbLteSocketConnectAddress;
  enbLteSocketConnectAddress.SetPhysicalAddress (Mac48Address::GetBroadcast ());
  enbLteSocketConnectAddress.SetSingleDevice (lteEnbNetDevice->GetIfIndex ());
  enbLteSocketConnectAddress.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
  retval = enbLteSocket->Connect (enbLteSocketConnectAddress);
  NS_ASSERT (retval == 0);  
  

  NS_LOG_INFO ("create EpcEnbApplication");
  Ptr<EpcEnbApplication> enbApp = CreateObject<EpcEnbApplication> (enbLteSocket, enbS1uSocket, enbAddress, sgwAddress, cellId);
  enb->AddApplication (enbApp);
  NS_ASSERT (enb->GetNApplications () == 1);
  NS_ASSERT_MSG (enb->GetApplication (0)->GetObject<EpcEnbApplication> () != 0, "cannot retrieve EpcEnbApplication");
  NS_LOG_LOGIC ("enb: " << enb << ", enb->GetApplication (0): " << enb->GetApplication (0));

  
  NS_LOG_INFO ("Create EpcX2 entity");
  Ptr<EpcX2> x2 = CreateObject<EpcX2> ();
  enb->AggregateObject (x2);

  NS_LOG_INFO ("connect S1-AP interface");
  m_mme->AddEnb (cellId, enbAddress, enbApp->GetS1apSapEnb ());
  m_sgwPgwApp->AddEnb (cellId, enbAddress, sgwAddress);
  enbApp->SetS1apSapMme (m_mme->GetS1apSapMme ());
}



void
OvsPointToPointEpcHelper::AddX2Interface (Ptr<Node> enb1, Ptr<Node> enb2)
{
  NS_LOG_FUNCTION (this << enb1 << enb2);

  // Create a point to point link between the two eNBs with
  // the corresponding new NetDevices on each side
  NodeContainer enbNodes;
  enbNodes.Add (enb1);
  enbNodes.Add (enb2);
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (m_x2LinkDataRate));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (m_x2LinkMtu));
  p2ph.SetChannelAttribute ("Delay", TimeValue (m_x2LinkDelay));
  NetDeviceContainer enbDevices = p2ph.Install (enb1, enb2);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #1 after installing p2p dev: " << enb1->GetObject<Ipv4> ()->GetNInterfaces ());
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #2 after installing p2p dev: " << enb2->GetObject<Ipv4> ()->GetNInterfaces ());
  Ptr<NetDevice> enb1Dev = enbDevices.Get (0);
  Ptr<NetDevice> enb2Dev = enbDevices.Get (1);

  m_x2Ipv4AddressHelper.NewNetwork ();


  Ipv4InterfaceContainer enbIpIfaces = m_x2Ipv4AddressHelper.Assign (enbDevices);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #1 after assigning Ipv4 addr to X2 dev: " << enb1->GetObject<Ipv4> ()->GetNInterfaces ());
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #2 after assigning Ipv4 addr to X2 dev: " << enb2->GetObject<Ipv4> ()->GetNInterfaces ());

  Ipv4Address enb1X2Address = enbIpIfaces.GetAddress (0);
  Ipv4Address enb2X2Address = enbIpIfaces.GetAddress (1);

  // Add X2 interface to both eNBs' X2 entities
  Ptr<EpcX2> enb1X2 = enb1->GetObject<EpcX2> ();
  Ptr<LteEnbNetDevice> enb1LteDev = enb1->GetDevice (0)->GetObject<LteEnbNetDevice> ();
  uint16_t enb1CellId = enb1LteDev->GetCellId ();
  NS_LOG_LOGIC ("LteEnbNetDevice #1 = " << enb1LteDev << " - CellId = " << enb1CellId);

  Ptr<EpcX2> enb2X2 = enb2->GetObject<EpcX2> ();
  Ptr<LteEnbNetDevice> enb2LteDev = enb2->GetDevice (0)->GetObject<LteEnbNetDevice> ();
  uint16_t enb2CellId = enb2LteDev->GetCellId ();
  NS_LOG_LOGIC ("LteEnbNetDevice #2 = " << enb2LteDev << " - CellId = " << enb2CellId);

  enb1X2->AddX2Interface (enb1CellId, enb1X2Address, enb2CellId, enb2X2Address);
  enb2X2->AddX2Interface (enb2CellId, enb2X2Address, enb1CellId, enb1X2Address);

  enb1LteDev->GetRrc ()->AddX2Neighbour (enb2LteDev->GetCellId ());
  enb2LteDev->GetRrc ()->AddX2Neighbour (enb1LteDev->GetCellId ());
}


void 
OvsPointToPointEpcHelper::AddUe (Ptr<NetDevice> ueDevice, uint64_t imsi)
{
  NS_LOG_FUNCTION (this << imsi << ueDevice );
  
  m_mme->AddUe (imsi);
  m_sgwPgwApp->AddUe (imsi);
  

}

uint8_t
OvsPointToPointEpcHelper::ActivateEpsBearer (Ptr<NetDevice> ueDevice, uint64_t imsi, Ptr<EpcTft> tft, EpsBearer bearer)
{
  NS_LOG_FUNCTION (this << ueDevice << imsi);

  // we now retrieve the IPv4 address of the UE and notify it to the SGW;
  // we couldn't do it before since address assignment is triggered by
  // the user simulation program, rather than done by the EPC   
  Ptr<Node> ueNode = ueDevice->GetNode (); 
  Ptr<Ipv4> ueIpv4 = ueNode->GetObject<Ipv4> ();
  NS_ASSERT_MSG (ueIpv4 != 0, "UEs need to have IPv4 installed before EPS bearers can be activated");
  int32_t interface =  ueIpv4->GetInterfaceForDevice (ueDevice);
  NS_ASSERT (interface >= 0);
  NS_ASSERT (ueIpv4->GetNAddresses (interface) == 1);
  Ipv4Address ueAddr = ueIpv4->GetAddress (interface, 0).GetLocal ();
  NS_LOG_LOGIC (" UE IP address: " << ueAddr);  m_sgwPgwApp->SetUeAddress (imsi, ueAddr);
  
  uint8_t bearerId = m_mme->AddBearer (imsi, tft, bearer);
  Ptr<LteUeNetDevice> ueLteDevice = ueDevice->GetObject<LteUeNetDevice> ();
  if (ueLteDevice)
    {
      ueLteDevice->GetNas ()->ActivateEpsBearer (bearer, tft);
    }
  return bearerId;
}

uint8_t
OvsPointToPointEpcHelper::ActivateEpsBearer (Ptr<NetDevice> ueLteDevice, Ptr<EpcUeNas> ueNas, uint64_t imsi, Ptr<EpcTft> tft, EpsBearer bearer)
{
	std::cout << "dummy ActivateEpsBearer" << std::endl;
	return 0;
}

Ptr<Node>
OvsPointToPointEpcHelper::GetPgwNode ()
{
  return m_sgwPgw;
}


Ipv4InterfaceContainer 
OvsPointToPointEpcHelper::AssignUeIpv4Address (NetDeviceContainer ueDevices)
{
  return m_ueAddressHelper.Assign (ueDevices);
}



Ipv4Address
OvsPointToPointEpcHelper::GetUeDefaultGatewayAddress ()
{
  // return the address of the tun device
  return m_sgwPgw->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
}


} // namespace ns3

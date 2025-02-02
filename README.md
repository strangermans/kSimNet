# 5G Network Simulator (5G K-SimNet)
5G K-SimNet is implemented based on [ns-3](https://www.nsnam.org "ns-3 Website") and [ns-3 mmWave module](https://github.com/nyuwireless/ns3-mmwave "mmWave github"). ns-3 is one of representative network simulators in communication and computer network research. ns-3 models network entities and structs complete set of the network protocol stack, so it can predict network behaviors more accurately. However, conventional ns-3 lacks of consideration about virtualization effects or cutting edge of software-defined network (SDN) technologies, or support of multi-connectivity of 5G networks. For simulating 5G network system, it is necessary to see virtualized effects or SDN functions, or test multi-connectivity scenario for deployment of 5G network. Our 5G K-SimNet includes OpenFlow controller, OpenFlow switches, modules for evaluating virtualization effects and also includes mmWave module for evaluating multi-connectivity of 5G network. Users who want to simulator 5G network system can evaluate their own network system topology with our 5G K-SimNet.

## How to Install
OS: Linux (ubuntu 16.04, 18.04)
1. Download source code from github.
2. Run "./configure.sh".

## Features
1. Performance metrics
- User throughput: User throughput is an important metric for network simulators, while it can show general network performance for each user device.
- User round trip time: User RTT can show timely change of packet transfer performance. If RTT increase/decrease sharply in short period of time, there might be special reasons such as VNF scaling, and so on.
- TCP congestion window size: With TCP congestion window size, user can verify the operation of network simulator and changes of RTT or throughput.
- Scaling delay: Scaling delay means additional delay components compared to non-virtualized core networks. Additional delays depending on where network operators put VNFs (VNF topology) can be evaluated.
- SDN switch throughput per port: With SDN controller and switches, user can engineer traffic flowing the simulation networks. SDN switch throughput per port can show results of traffic engineering.

2. Simulation parameters for configuration

2.1 General parameters 
- X2 interface settings (src/lte/helper$ point-to-point-epc-helper.cc )
  + X2 link data rate (m_x2LinkDataRate)
  + X2 link delay (m_x2LinkDelay)
  + MTU of X2 link (m_x2LinkMtu)
- S1AP interface settings (src/lte/helper$ point-to-point-epc-helper.cc )
  + S1AP link data rate (m_s1apLinkDataRate)
  + S1AP link delay (m_s1apLinkDelay)
  + MTU of S1AP link (m_s1apLinkMtu)
- S1-U interface settings (src/lte/helper$ point-to-point-epc-helper.cc )
  + S1-U link data rate (m_s1uLinkDataRate)
  + S1-U link delay (m_s1uLinkDelay)
  + MTU of S1-U link (m_s1uLinkMtu)

2.2. 5G NR NSA (src/lte/model$ mc-enb-pdcp.cc)
- PDCP reordering settings (mc-ue-pdcp.cc)
	+ PDCP reordering enabler ( m_isEnableReordering)
	+ PDCP reordering timer (expiredTime)
- Type of splitting algorithms(m_isSplitting)
	+ Splitting to single eNB (0 or 1)
	+ Alternative splitting scheme to two eNB (2)
	+ Assistant info-based splitting scheme (3,4)

2.3. 5G NR Channel
- Operating band parameters (src/mmwave/model$ mmwave-phy-mac-common.cc)
	+ Center frequency (m_centerFrequency)
- Channel parameters (src/mmwave/model$ mmwave-3gpp-propagation-loss-model.cc)
	+ Pathloss model (&MmWaveHelper::SetPathlossModel)
- Scenario (m_scenario)
- Channel condition (m_channelConditions)
- Optional Nlos (m_optionNlosEnabled)
- Shadowing (m_shadowingEnabled)
  + Frame structure and physical resources parameters (src/mmwave/model$ mmwave-phy-mac-common.cc)
- Channel bandwidth (m_numRb)
- Symbols per slot (m_symbolsPerSlot)
- Slot length (m_slotsPerSubframe)
- The number of layer in gNB (m_numEnbLayers)
  + gNB-side parameters (src/mmwave/model$ mmwave-enb-phy.cc)
- Tx power (m_txPower)
	+ UE-side parameters (src/mmwave/model$ mmwave-ue-phy.cc)
- TxPower (m_txPower)
  + gNB antenna parameters (src/mmwave/model$ mmwave-enb-net-device.cc)
- The number of vertical antenna elements (m_vAntennaNum)
- The number of horizontal antenna elements (m_hAntennaNum)
- The number of polarization dimensions (m_polarNum)
- The number of vertical TXRUs (m_vTxruNum)
- The number of horizontal TXRUs (m_hTxruNum)
- Antenna connection mode (m_connectMode)
- Bearing angle (m_rotation.x)
- Downtilt angle (m_rotation.y)
- Slant angle (m_rotation.z)
- Polarization slant angle (0 or 45 degree) (m_pol)
  + UE antenna parameters (src/mmwave/model$ mmwave-ue-net-device.cc)
- The number of vertical antenna elements (m_vAntennaNum)
- The number of horizontal antenna elements (m_hAntennaNum)
- The number of polarization dimensions (m_polarNum)
- The number of vertical TXRUs (m_vTxruNum)
- The number of horizontal TXRUs (m_hTxruNum)
- Antenna connection mode (m_connectMode)
- Bearing angle (m_rotation.x)
- Downtilt angle (m_rotation.y)
- Slant angle (m_rotation.z)
- Polarization slant angle (0 or 45 degree) (m_pol)
	+ MC UE antenna parameters (src/mmwave/model$ mc-ue-net-device.cc)
- The number of vertical antenna elements (m_vAntennaNum)
- The number of horizontal antenna elements (m_hAntennaNum)
- The number of polarization dimensions (m_polarNum)
- The number of vertical TXRUs (m_vTxruNum)
- The number of horizontal TXRUs (m_hTxruNum)
- Antenna connection mode (m_connectMode)
- Bearing angle (m_rotation.x)
- Downtilt angle (m_rotation.y)
- Slant angle (m_rotation.z)
- Polarization slant angle (0 or 45 degree) (m_pol)

2.4. 5G Core
- AMF-related parameters (virt-5gc.cc, virt-5gc-node.cc, topology input file (user input))
	+ AMF cpu capacity (Virt5gcNode::cpuSize)
	+ AMF memory capacity (Virt5gcNode::memSize)
	+ AMF disk capacity (Virt5gcNode::diskSize)
	+ AMF bandwidth (Virt5gcNode::bwSize)
	+ AMF workloads (Virt5gcNode::cpuUtil, Virt5gcNode::memUtil, Virt5gcNode::diskUtil, Virt5gcNode::bwUtil)
	+ AMF x axis (Virt5gcNode::node_x)
	+ AMF y axis (Virt5gcNode::node_y)
- SMF-related parameters (virt-5gc.cc, virt-5gc-node.cc, topology input file (user input))
	+ SMF cpu capacity (Virt5gcNode::cpuSize)
	+ SMF memory capacity (Virt5gcNode::memSize)
	+ SMF disk capacity (Virt5gcNode::diskSize)
	+ SMF bandwidth (Virt5gcNode::bwSize)
	+ SMF workloads (Virt5gcNode::cpuUtil, Virt5gcNode::memUtil, Virt5gcNode::diskUtil, Virt5gcNode::bwUtil)
	+ SMF x axis (Virt5gcNode::node_x)
	+ SMF y axis (Virt5gcNode::node_y)
- UPF-related parameters (virt-5gc.cc, virt-5gc-node.cc, topology input file (user input))
	+ UPF cpu capacity (Virt5gcNode::cpuSize)
	+ UPF memory capacity (Virt5gcNode::memSize)
	+ UPF disk capacity (Virt5gcNode::diskSize)
	+ UPF bandwidth (Virt5gcNode::bwSize)
	+ UPF workloads (Virt5gcNode::cpuUtil, Virt5gcNode::memUtil, Virt5gcNode::diskUtil, Virt5gcNode::bwUtil)
	+ UPF x axis (Virt5gcNode::node_x)
	+ UPF y axis (Virt5gcNode::node_y)
- Virtualization parameters (virt-5gc.cc)
	+ Scaling policy (memory split ratio) (Virt5gc::scaleInRate, Virt5gc::scaleOutRate)
	+ VM provisioning delay (Virt5gc::allocDelay)
- Data center parameters (virt-5gc-node.cc, virt-5gc-vm.cc, topology input file (user input) )
	+ Data center topology
- Node ID (Virt5gcNode::node_id)
- Node ToR ID (Virt5gcVm::ToR)
- Node physical machine ID (Virt5gcVm::pm)
- Link bandwidth (Topology input file)
- Link edge nodes (Topology input file)
- Software-Defined Networking (SDN) parameters (User input file (scratch file), ovs-point-to-point-epc-helper.cc, qos-controller.cc )
	+ Inter-switch data rate (ovs-point-to-point-epc-helper.cc (DataRate(“10Mbps”)) )
	+ gNB-Switch-GW data rate (ovs-point-to-point-epc-helper.cc (ovs-point-to-point-epc-helper.cc (m_s1uLinkDataRate) )
	+ remote host-GW data rate (scratch file (DataRate) )
	+ In-port, out-port (qos-controller.cc (in_port, output) )
	+ Output port group (qos-controller.cc (group-mode) )
	+ QoS weight (qos-controller.cc (weight) )

## Authors
5G K-SimNet is the result of the development effort carried out by different people at **Seoul National University** in South Korea. The main contributors are listed below.

### Network Laboratory (NETLAB) http://netlab.snu.ac.kr/
- Dongyeon Woo <dywoo@netlab.snu.ac.kr>
- Seongjoon Kang <sjkang@netlab.snu.ac.kr>
- Siyoung Choi <sychoi@netlab.snu.ac.kr>
- Saewoong Bahk <sbahk@snu.ac.kr>

### Multimedia & Wireless Networking Laboratory (MWNL) http://www.mwnl.snu.ac.kr/
- Junseok Kim <jskim14@mwnl.snu.ac.kr>
- Suhun Lim <shlim@mwnl.snu.ac.kr>
- Sunghyun Choi <schoi@snu.ac.kr>

### Multimedia and Mobile Communications Laboratory (MMLAB) http://mmlab.snu.ac.kr
- Junghwan Song <jhsong@mwnl.snu.ac.kr>
- Taekyoung (Ted) Kwon <tkkwon@snu.ac.kr>

## Acknowledgement
This work was supported by "The Cross-Ministry Giga KOREA Project" grant funded by the Korea government (MSIT) (No. GK18S0400, Research and Development of Open 5G Reference Model).

/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *   Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
 *  
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *  
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *  
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *  
 *  
 *   Author: Marco Mezzavilla < mezzavilla@nyu.edu>
 *        	 Sourjya Dutta <sdutta@nyu.edu>
 *        	 Russell Ford <russell.ford@nyu.edu>
 *        	 Menglei Zhang <menglei@nyu.edu>
 */

#include "mmwave-3gpp-channel.h"
#include <ns3/log.h>
#include <ns3/math.h>
#include <ns3/simulator.h>
#include <ns3/mmwave-phy.h>
#include <ns3/mmwave-net-device.h>
#include <ns3/node.h>
#include <ns3/mmwave-ue-net-device.h>
#include <ns3/mc-ue-net-device.h>
#include <ns3/mmwave-enb-net-device.h>
#include <ns3/mmwave-ue-phy.h>
#include <ns3/mmwave-enb-phy.h>
#include <ns3/double.h>
#include <algorithm>
#include <random> // std::default_random_engine
#include <ns3/boolean.h>
#include <ns3/integer.h>
#include "mmwave-spectrum-value-helper.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MmWave3gppChannel");

NS_OBJECT_ENSURE_REGISTERED(MmWave3gppChannel);

//Table 7.5-3: Ray offset angles within a cluster, given for rms angle spread normalized to 1.
static const double offSetAlpha[20] = {
	0.0447, -0.0447, 0.1413, -0.1413, 0.2492, -0.2492, 0.3715, -0.3715, 0.5129, -0.5129, 0.6797, -0.6797, 0.8844, -0.8844, 1.1481, -1.1481, 1.5195, -1.5195, 2.1551, -2.1551};

/*
 * The cross correlation matrix is constructed according to table 7.5-6.
 * All the square root matrix is being generated using the Cholesky decomposition
 * and following the order of [SF,K,DS,ASD,ASA,ZSD,ZSA].
 * The parameter K is ignored in NLOS.
 *
 * The Matlab file to generate the matrices can be found in the mmwave/model/BeamFormingMatrix/SqrtMatrix.m
 * */

// Change the matrices(sqrtC_RMa_LOS, sqrtC_RMA_NLOS, sqrtC_UMa_O2I, sqrtC_UMi_O2I, sqrtC_office_LOS, sqrtC_office_NLOS) according to the TR 38.901 - 2018.06.04 shlim

static const double sqrtC_RMa_LOS[7][7] = {
	{1, 0, 0, 0, 0, 0, 0},
	{0, 1, 0, 0, 0, 0, 0},
	{-0.5, 0, 0.866025, 0, 0, 0, 0},
	{0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 1, 0, 0},
	{0.01, 0, -0.051962, 0.73, -0.2, 0.651383, 0},
	{-0.17, -0.02, 0.213620, -0.14, 0.24, 0.142773, 0.909661}};

static const double sqrtC_RMa_O2I[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{0, 1, 0, 0, 0, 0},
	{0, 0, 1, 0, 0, 0},
	{0, 0, -0.7, 0.714143, 0, 0},
	{0, 0, 0.66, -0.123225, 0.741091, 0},
	{0, 0, 0.47, 0.152631, -0.393194, 0.775373}};

static const double sqrtC_RMa_NLOS[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{-0.5, 0.866025, 0, 0, 0, 0},
	{0.6, -0.115470, 0.791623, 0, 0, 0},
	{0, 0, 0, 1, 0, 0},
	{-0.04, -0.138564, 0.540662, -0.18, 0.809003, 0},
	{-0.25, -0.606218, -0.240013, 0.26, -0.231685, 0.625392}};

static const double sqrtC_UMa_LOS[7][7] = {
	{1, 0, 0, 0, 0, 0, 0},
	{0, 1, 0, 0, 0, 0, 0},
	{-0.4, -0.4, 0.824621, 0, 0, 0, 0},
	{-0.5, 0, 0.242536, 0.83137, 0, 0, 0},
	{-0.5, -0.2, 0.630593, -0.484671, 0.278293, 0, 0},
	{0, 0, -0.242536, 0.672172, 0.642214, 0.27735, 0},
	{-0.8, 0, -0.388057, -0.367926, 0.238537, -3.58949e-15, 0.130931}};

static const double sqrtC_UMa_NLOS[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{-0.4, 0.916515, 0, 0, 0, 0},
	{-0.6, 0.174574, 0.78072, 0, 0, 0},
	{0, 0.654654, 0.365963, 0.661438, 0, 0},
	{0, -0.545545, 0.762422, 0.118114, 0.327327, 0},
	{-0.4, -0.174574, -0.396459, 0.392138, 0.49099, 0.507445}};

static const double sqrtC_UMa_O2I[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{-0.5, 0.866025, 0, 0, 0, 0},
	{0.2, 0.577350, 0.791623, 0, 0, 0},
	{0, 0.461880, -0.336861, 0.820482, 0, 0},
	{0, -0.692820, 0.252646, 0.493742, 0.460857, 0},
	{0, -0.230941, 0.168431, 0.808554, -0.220827, 0.464515}

};

static const double sqrtC_UMi_LOS[7][7] = {
	{1, 0, 0, 0, 0, 0, 0},
	{0.5, 0.866025, 0, 0, 0, 0, 0},
	{-0.4, -0.57735, 0.711805, 0, 0, 0, 0},
	{-0.5, 0.057735, 0.468293, 0.726201, 0, 0, 0},
	{-0.4, -0.11547, 0.805464, -0.23482, 0.350363, 0, 0},
	{0, 0, 0, 0.688514, 0.461454, 0.559471, 0},
	{0, 0, 0.280976, 0.231921, -0.490509, 0.11916, 0.782603}};

static const double sqrtC_UMi_NLOS[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{-0.7, 0.714143, 0, 0, 0, 0},
	{0, 0, 1, 0, 0, 0},
	{-0.4, 0.168034, 0, 0.90098, 0, 0},
	{0, -0.70014, 0.5, 0.130577, 0.4927, 0},
	{0, 0, 0.5, 0.221981, -0.566238, 0.616522}};

static const double sqrtC_UMi_O2I[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{-0.5, 0.866025, 0, 0, 0, 0},
	{0.2, 0.577350, 0.791623, 0, 0, 0},
	{0, 0.461880, -0.336861, 0.820482, 0, 0},
	{0, -0.692820, 0.252646, 0.493742, 0.460857, 0},
	{0, -0.230941, 0.168431, 0.808554, -0.220827, 0.464515}};

static const double sqrtC_office_LOS[7][7] = {
	{1, 0, 0, 0, 0, 0, 0},
	{0.5, 0.866025, 0, 0, 0, 0, 0},
	{-0.8, -0.11547, 0.588784, 0, 0, 0, 0},
	{-0.4, 0.23094, 0.520847, 0.717903, 0, 0, 0},
	{-0.5, 0.288675, 0.73598, -0.348236, 0.0610847, 0, 0},
	{0.2, -0.11547, 0.418943, 0.541106, 0.219905, 0.655744, 0},
	{0.3, -0.057735, 0.735980, -0.348236, 0.061085, -0.304997, 0.383375}};

static const double sqrtC_office_NLOS[6][6] = {
	{1, 0, 0, 0, 0, 0},
	{-0.5, 0.866025, 0, 0, 0, 0},
	{0, 0.46188, 0.886942, 0, 0, 0},
	{-0.4, -0.23094, 0.1202634, 0.878751, 0, 0},
	{0, -0.311769, 0.555697, -0.249198, 0.728344, 0},
	{0, -0.069282, 0.295397, 0.430696, 0.468462, 0.709215}};

MmWave3gppChannel::MmWave3gppChannel()
{
	m_uniformRv = CreateObject<UniformRandomVariable>();
	m_uniformRvBlockage = CreateObject<UniformRandomVariable>();
	m_expRv = CreateObject<ExponentialRandomVariable>();
	m_normalRv = CreateObject<NormalRandomVariable>();
	m_normalRv->SetAttribute("Mean", DoubleValue(0));
	m_normalRv->SetAttribute("Variance", DoubleValue(1));
	m_normalRvBlockage = CreateObject<NormalRandomVariable>();
	m_normalRvBlockage->SetAttribute("Mean", DoubleValue(0));
	m_normalRvBlockage->SetAttribute("Variance", DoubleValue(1));
	m_forceInitialBfComputation = false;
}

TypeId
MmWave3gppChannel::GetTypeId(void)
{
	static TypeId tid = TypeId("ns3::MmWave3gppChannel")
							.SetParent<Object>()
							.AddAttribute("UpdatePeriod",
										  "Enable spatially-consistent UT mobility modeling procedure A, the update period unit is in ms, set to 0 ms to disable update",
										  TimeValue(NanoSeconds(0)),
										  MakeTimeAccessor(&MmWave3gppChannel::m_updatePeriod),
										  MakeTimeChecker())
							//180821-jskim14-set analog beamforming vector update period
							.AddAttribute("AnalogBeamPeriod",
										  "Analog bemaforming vector update period, the update period unit is in ms, set to 0 ms to disable update",
										  TimeValue(MilliSeconds(0)),
										  MakeTimeAccessor(&MmWave3gppChannel::m_analogBeamPeriod),
										  MakeTimeChecker())
							.AddAttribute("CellScan",
										  "Use beam search method to determine beamforming vector, the default is long-term covariance matrix method",
										  BooleanValue(false),
										  MakeBooleanAccessor(&MmWave3gppChannel::m_cellScan),
										  MakeBooleanChecker())
							.AddAttribute("Blockage",
										  "Enable blockage model A (sec 7.6.4.1)",
										  BooleanValue(false),
										  MakeBooleanAccessor(&MmWave3gppChannel::m_blockage),
										  MakeBooleanChecker())
							.AddAttribute("NumNonselfBlocking",
										  "number of non-self-blocking regions",
										  IntegerValue(4),
										  MakeIntegerAccessor(&MmWave3gppChannel::m_numNonSelfBloking),
										  MakeIntegerChecker<uint16_t>())
							.AddAttribute("BlockerSpeed",
										  "The speed of moving blockers, the unit is m/s",
										  DoubleValue(1),
										  MakeDoubleAccessor(&MmWave3gppChannel::m_blockerSpeed),
										  MakeDoubleChecker<double>())
							.AddAttribute("PortraitMode",
										  "true for portrait mode, false for landscape mode",
										  BooleanValue(true),
										  MakeBooleanAccessor(&MmWave3gppChannel::m_portraitMode),
										  MakeBooleanChecker())
							.AddAttribute("CrossPolarization",
										  "The existence of cross-polarization",
										  BooleanValue(true),
										  MakeBooleanAccessor(&MmWave3gppChannel::m_crossPolar),
										  MakeBooleanChecker());
	return tid;
} // We establish the cross-polarization attribute. 2018.07.03 shlim.

MmWave3gppChannel::~MmWave3gppChannel()
{
}

void MmWave3gppChannel::DoDispose()
{
	NS_LOG_FUNCTION(this);
}

void MmWave3gppChannel::SetConfigurationParameters(Ptr<MmWavePhyMacCommon> ptrConfig)
{
	m_phyMacConfig = ptrConfig;
}

Ptr<MmWavePhyMacCommon>
MmWave3gppChannel::GetConfigurationParameters(void) const
{
	return m_phyMacConfig;
}

void MmWave3gppChannel::ConnectDevices(Ptr<NetDevice> dev1, Ptr<NetDevice> dev2)
{
	key_t key = std::make_pair(dev1, dev2);

	std::map<key_t, int>::iterator iter = m_connectedPair.find(key);
	if (iter != m_connectedPair.end())
	{
		m_connectedPair.erase(iter);
	}
	m_connectedPair.insert(std::make_pair(key, 1));
}

void MmWave3gppChannel::SetBeamformingVector(Ptr<NetDevice> ueDevice, Ptr<NetDevice> enbDevice)
{
	key_t key = std::make_pair(ueDevice, enbDevice);
	Ptr<MmWaveEnbNetDevice> EnbDev =
		DynamicCast<MmWaveEnbNetDevice>(enbDevice);
	Ptr<MmWaveUeNetDevice> UeDev =
		DynamicCast<MmWaveUeNetDevice>(ueDevice);
	//180706-jskim14-consider NR tx mode
	if (m_nrTxMode == 1)
	{
		uint8_t connectMode = EnbDev->GetConnectMode();
		uint8_t vAntNum = EnbDev->GetVAntennaNum();
		uint8_t hAntNum = EnbDev->GetHAntennaNum();
		uint8_t polarNum = EnbDev->GetPolarNum();
		uint8_t vTxruNum = EnbDev->GetVTxruNum();
		uint8_t hTxruNum = EnbDev->GetHTxruNum();
		Vector rotation = EnbDev->GetRotation();
		double pol = EnbDev->GetPolarization();
		Ptr<AntennaArrayModel> enbAntennaArray = DynamicCast<AntennaArrayModel>(
			EnbDev->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		enbAntennaArray->SetAntParams(connectMode, vAntNum, hAntNum, polarNum, vTxruNum, hTxruNum, enbDevice);
		enbAntennaArray->SetAntennaRotation(rotation.x, rotation.y, rotation.z, pol);

		if (UeDev != 0)
		{
			connectMode = UeDev->GetConnectMode();
			vAntNum = UeDev->GetVAntennaNum();
			hAntNum = UeDev->GetHAntennaNum();
			polarNum = UeDev->GetPolarNum();
			vTxruNum = UeDev->GetVTxruNum();
			hTxruNum = UeDev->GetHTxruNum();
			rotation = UeDev->GetRotation();
			pol = UeDev->GetPolarization();

			Ptr<AntennaArrayModel> ueAntennaArray = DynamicCast<AntennaArrayModel>(
				UeDev->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
			ueAntennaArray->SetAntParams(connectMode, vAntNum, hAntNum, polarNum, vTxruNum, hTxruNum, ueDevice);
			ueAntennaArray->SetAntennaRotation(rotation.x, rotation.y, rotation.z, pol);
		}
		else
		{
			Ptr<McUeNetDevice> UeDev =
				DynamicCast<McUeNetDevice>(ueDevice);
			if (UeDev != 0)
			{
				connectMode = UeDev->GetConnectMode();
				vAntNum = UeDev->GetVAntennaNum();
				hAntNum = UeDev->GetHAntennaNum();
				polarNum = UeDev->GetPolarNum();
				vTxruNum = UeDev->GetVTxruNum();
				hTxruNum = UeDev->GetHTxruNum();
				rotation = UeDev->GetRotation();
				pol = UeDev->GetPolarization();

				Ptr<AntennaArrayModel> ueAntennaArray;
				if (isAdditionalMmWavePhy)
				{
					ueAntennaArray = DynamicCast<AntennaArrayModel>(
						UeDev->GetMmWavePhy_2()->GetDlSpectrumPhy()->GetRxAntenna());
					ueAntennaArray->SetAntParams(connectMode, vAntNum, hAntNum, polarNum, vTxruNum, hTxruNum, ueDevice);
					ueAntennaArray->SetAntennaRotation(rotation.x, rotation.y, rotation.z, pol);
				}
				else
				{
					ueAntennaArray = DynamicCast<AntennaArrayModel>(
						UeDev->GetMmWavePhy()->GetDlSpectrumPhy()->GetRxAntenna());
					ueAntennaArray->SetAntParams(connectMode, vAntNum, hAntNum, polarNum, vTxruNum, hTxruNum, ueDevice);
					ueAntennaArray->SetAntennaRotation(rotation.x, rotation.y, rotation.z, pol);
				}
			}
			else
			{
				NS_FATAL_ERROR("Unrecognized pair of devices");
			}
		}
	}
	else
	{
		//NYU original code
		if (UeDev != 0)
		{
			//NS_LOG_UNCOND("SetBeamformingVector between UE " << ueDevice << " and enbDevice " << enbDevice);
			Ptr<AntennaArrayModel> ueAntennaArray = DynamicCast<AntennaArrayModel>(
				UeDev->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
			Ptr<AntennaArrayModel> enbAntennaArray = DynamicCast<AntennaArrayModel>(
				EnbDev->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
			complexVector_t dummy;
			ueAntennaArray->SetBeamformingVector(dummy, enbDevice);
			enbAntennaArray->SetBeamformingVector(dummy, ueDevice);
		}
		else
		{
			Ptr<McUeNetDevice> UeDev =
				DynamicCast<McUeNetDevice>(ueDevice);
			if (UeDev != 0)
			{
				//NS_LOG_UNCOND("SetBeamformingVector between UE " << ueDevice << " and enbDevice " << enbDevice);
				Ptr<AntennaArrayModel> ueAntennaArray;
				if (isAdditionalMmWavePhy) //sjkang11125
					ueAntennaArray = DynamicCast<AntennaArrayModel>(
						UeDev->GetMmWavePhy_2()->GetDlSpectrumPhy()->GetRxAntenna());
				else
					ueAntennaArray = DynamicCast<AntennaArrayModel>(
						UeDev->GetMmWavePhy()->GetDlSpectrumPhy()->GetRxAntenna());

				Ptr<AntennaArrayModel> enbAntennaArray = DynamicCast<AntennaArrayModel>(
					EnbDev->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
				complexVector_t dummy;
				ueAntennaArray->SetBeamformingVector(dummy, enbDevice);
				enbAntennaArray->SetBeamformingVector(dummy, ueDevice);
			}
			else
			{
				NS_FATAL_ERROR("Unrecognized pair of devices");
			}
		}
	}
	//jskim14-end
}

void MmWave3gppChannel::Initial(NetDeviceContainer ueDevices, NetDeviceContainer enbDevices)
{

	NS_LOG_INFO(&ueDevices << &enbDevices);

	m_forceInitialBfComputation = true;

	for (NetDeviceContainer::Iterator i = ueDevices.Begin(); i != ueDevices.End(); i++)
	{
		for (NetDeviceContainer::Iterator j = enbDevices.Begin(); j != enbDevices.End(); j++)
		{
			SetBeamformingVector(*i, *j);

			// get the mobility objects
			Ptr<const MobilityModel> a = (*j)->GetNode()->GetObject<MobilityModel>();
			Ptr<const MobilityModel> b = (*i)->GetNode()->GetObject<MobilityModel>();
			NS_LOG_INFO("a " << a << " b " << b);

			// initialize the pathloss and channel condition
			if (DynamicCast<MmWave3gppPropagationLossModel>(m_3gppPathloss) != 0)
			{
				m_3gppPathloss->GetObject<MmWave3gppPropagationLossModel>()
					->GetLoss(a->GetObject<MobilityModel>(), b->GetObject<MobilityModel>());
			} // the GetObject trick is a trick against the const keyword
			else if (DynamicCast<MmWave3gppBuildingsPropagationLossModel>(m_3gppPathloss) != 0)
			{
				m_3gppPathloss->GetObject<MmWave3gppBuildingsPropagationLossModel>()
					->GetLoss(a->GetObject<MobilityModel>(), b->GetObject<MobilityModel>());
			}
			else
			{
				NS_FATAL_ERROR("unknow pathloss model");
			}

			std::vector<int> listOfSubchannels;
			for (unsigned subChannelIndex = 0; subChannelIndex < m_phyMacConfig->GetTotalNumChunk(); subChannelIndex++)
			{
				listOfSubchannels.push_back(subChannelIndex);
			}

			Ptr<const SpectrumValue> fakePsd = MmWaveSpectrumValueHelper::CreateTxPowerSpectralDensity(m_phyMacConfig, 0, listOfSubchannels);
			DoCalcRxPowerSpectralDensity(fakePsd, a, b);

			// Ptr<MmWaveUeNetDevice> UeDev =
			// 			DynamicCast<MmWaveUeNetDevice> (*i);
			// if (UeDev != 0)
			// {

			// }
			// else
			// {
			// 	Ptr<McUeNetDevice> UeDev = DynamicCast<MmWaveUeNetDevice> (*i);
			// 	if (UeDev !=0)
			// }
			// if (UeDev->GetTargetEnb ())
			// {
			// 	Ptr<NetDevice> targetBs = UeDev->GetTargetEnb ();
			// 	ConnectDevices (*i, targetBs);
			// 	ConnectDevices (targetBs, *i);

			// }
		}
	}

	m_forceInitialBfComputation = false;
}

Ptr<SpectrumValue>
MmWave3gppChannel::DoCalcRxPowerSpectralDensity(Ptr<const SpectrumValue> txPsd,
												Ptr<const MobilityModel> a,
												Ptr<const MobilityModel> b) const
{
	NS_LOG_FUNCTION(this);
	Ptr<SpectrumValue> rxPsd = Copy(txPsd);

	Ptr<NetDevice> txDevice = a->GetObject<Node>()->GetDevice(0);
	Ptr<NetDevice> rxDevice = b->GetObject<Node>()->GetDevice(0);
	Ptr<MmWaveEnbNetDevice> txEnb =
		DynamicCast<MmWaveEnbNetDevice>(txDevice);
	Ptr<McUeNetDevice> rxMcUe =
		DynamicCast<McUeNetDevice>(rxDevice);
	Ptr<McUeNetDevice> txMcUe =
		DynamicCast<McUeNetDevice>(txDevice);
	Ptr<MmWaveUeNetDevice> rxUe =
		DynamicCast<MmWaveUeNetDevice>(rxDevice);

	bool downlink = false;
	bool downlinkMc = false;
	bool uplink = false;
	bool uplinkMc = false;

	/* txAntennaNum[0]-number of vertical antenna elements
	 * txAntennaNum[1]-number of horizontal antenna elements*/
	//180709-jskim14
	/* txAntennaNum[2]-number of polarization dimension (1 or 2)*/

	//180709-jskim14-extend array for adding polarization dimension
	int arraySize;
	if (m_nrTxMode == 1)
	{
		arraySize = 3;
	}
	else
	{
		arraySize = 2;
	}
	uint8_t txAntennaNum[arraySize];
	uint8_t rxAntennaNum[arraySize];
	//jskim14-end
	Ptr<AntennaArrayModel> txAntennaArray, rxAntennaArray;

	Vector locUT;
	if (txEnb != 0 && rxUe != 0 && rxMcUe == 0)
	{
		NS_LOG_INFO("this is downlink case, a tx " << a->GetPosition() << " b rx " << b->GetPosition());
		downlink = true;

		//180709-jskim14-consider NR tx mode
		if (m_nrTxMode == 1)
		{
			txAntennaNum[0] = txEnb->GetVAntennaNum();
			txAntennaNum[1] = txEnb->GetHAntennaNum();
			txAntennaNum[2] = txEnb->GetPolarNum();
			rxAntennaNum[0] = rxUe->GetVAntennaNum();
			rxAntennaNum[1] = rxUe->GetHAntennaNum();
			rxAntennaNum[2] = rxUe->GetPolarNum();
		}
		else
		{
			txAntennaNum[0] = sqrt(txEnb->GetAntennaNum());
			txAntennaNum[1] = sqrt(txEnb->GetAntennaNum());
			rxAntennaNum[0] = sqrt(rxUe->GetAntennaNum());
			rxAntennaNum[1] = sqrt(rxUe->GetAntennaNum());
		}
		//jskim14-end

		txAntennaArray = DynamicCast<AntennaArrayModel>(
			txEnb->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		rxAntennaArray = DynamicCast<AntennaArrayModel>(
			rxUe->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		locUT = b->GetPosition();
	}
	else if (txEnb != 0 && rxMcUe != 0 && rxUe == 0)
	{
		NS_LOG_INFO("this is MC downlink case, a tx " << a->GetPosition() << " b rx " << b->GetPosition());
		downlinkMc = true;

		//180709-jskim14-consider NR tx mode
		if (m_nrTxMode == 1)
		{
			txAntennaNum[0] = txEnb->GetVAntennaNum();
			txAntennaNum[1] = txEnb->GetHAntennaNum();
			txAntennaNum[2] = txEnb->GetPolarNum();
			rxAntennaNum[0] = rxMcUe->GetVAntennaNum();
			rxAntennaNum[1] = rxMcUe->GetHAntennaNum();
			rxAntennaNum[2] = rxMcUe->GetPolarNum();
		}
		else
		{
			txAntennaNum[0] = sqrt(txEnb->GetAntennaNum());
			txAntennaNum[1] = sqrt(txEnb->GetAntennaNum());
			rxAntennaNum[0] = sqrt(rxMcUe->GetAntennaNum());
			rxAntennaNum[1] = sqrt(rxMcUe->GetAntennaNum());
		}
		//jskim14-end

		txAntennaArray = DynamicCast<AntennaArrayModel>(
			txEnb->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		if (isAdditionalMmWavePhy)
			rxAntennaArray = DynamicCast<AntennaArrayModel>(
				rxMcUe->GetMmWavePhy_2()->GetDlSpectrumPhy()->GetRxAntenna());
		else
			rxAntennaArray = DynamicCast<AntennaArrayModel>(
				rxMcUe->GetMmWavePhy()->GetDlSpectrumPhy()->GetRxAntenna()); //sjkang1125
		locUT = b->GetPosition();
	}
	else if (txEnb == 0 && rxUe == 0 && txMcUe == 0 && rxMcUe == 0)
	{
		NS_LOG_INFO("this is uplink case, a tx " << a->GetPosition() << " b rx " << b->GetPosition());
		uplink = true;
		Ptr<MmWaveUeNetDevice> txUe =
			DynamicCast<MmWaveUeNetDevice>(txDevice);
		Ptr<MmWaveEnbNetDevice> rxEnb =
			DynamicCast<MmWaveEnbNetDevice>(rxDevice);

		//180709-jskim14-consider NR tx mode
		if (m_nrTxMode == 1)
		{
			txAntennaNum[0] = txUe->GetVAntennaNum();
			txAntennaNum[1] = txUe->GetHAntennaNum();
			txAntennaNum[2] = txUe->GetPolarNum();
			rxAntennaNum[0] = rxEnb->GetVAntennaNum();
			rxAntennaNum[1] = rxEnb->GetHAntennaNum();
			rxAntennaNum[2] = rxEnb->GetPolarNum();
		}
		else
		{
			txAntennaNum[0] = sqrt(txUe->GetAntennaNum());
			txAntennaNum[1] = sqrt(txUe->GetAntennaNum());
			rxAntennaNum[0] = sqrt(rxEnb->GetAntennaNum());
			rxAntennaNum[1] = sqrt(rxEnb->GetAntennaNum());
		}
		//jskim14-end

		txAntennaArray = DynamicCast<AntennaArrayModel>(
			txUe->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		rxAntennaArray = DynamicCast<AntennaArrayModel>(
			rxEnb->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		locUT = a->GetPosition();
	}
	else if (txEnb == 0 && rxUe == 0 && txMcUe != 0 && rxMcUe == 0)
	{
		NS_LOG_INFO("this is MC uplink case, a tx " << a->GetPosition() << " b rx " << b->GetPosition());
		uplinkMc = true;
		Ptr<MmWaveEnbNetDevice> rxEnb =
			DynamicCast<MmWaveEnbNetDevice>(rxDevice);

		//180709-jskim14-consider NR tx mode
		if (m_nrTxMode == 1)
		{
			txAntennaNum[0] = txMcUe->GetVAntennaNum();
			txAntennaNum[1] = txMcUe->GetHAntennaNum();
			txAntennaNum[2] = txMcUe->GetPolarNum();
			rxAntennaNum[0] = rxEnb->GetVAntennaNum();
			rxAntennaNum[1] = rxEnb->GetHAntennaNum();
			rxAntennaNum[2] = rxEnb->GetPolarNum();
		}
		else
		{
			txAntennaNum[0] = sqrt(txMcUe->GetAntennaNum());
			txAntennaNum[1] = sqrt(txMcUe->GetAntennaNum());
			rxAntennaNum[0] = sqrt(rxEnb->GetAntennaNum());
			rxAntennaNum[1] = sqrt(rxEnb->GetAntennaNum());
		}
		//jskim14-end

		if (isAdditionalMmWavePhy) //sjkang1125
			txAntennaArray = DynamicCast<AntennaArrayModel>(
				txMcUe->GetMmWavePhy_2()->GetDlSpectrumPhy()->GetRxAntenna());
		else
			txAntennaArray = DynamicCast<AntennaArrayModel>(
				txMcUe->GetMmWavePhy()->GetDlSpectrumPhy()->GetRxAntenna());

		rxAntennaArray = DynamicCast<AntennaArrayModel>(
			rxEnb->GetPhy()->GetDlSpectrumPhy()->GetRxAntenna());
		locUT = a->GetPosition();
	}
	else
	{
		NS_LOG_INFO("enb to enb or ue to ue transmission, skip beamforming a tx " << a->GetPosition() << " b rx " << b->GetPosition());
		return rxPsd;
	}

	//180813-jskim14-move this condition to the after channelParams declaration
	/*if (txAntennaArray->IsOmniTx() || rxAntennaArray->IsOmniTx())
	{
		if (txEnb != 0 && rxMcUe != 0 && rxUe == 0)
		{
			if(m_nrTxMode==1)
			{
				//NS_LOG_UNCOND("Omni transmission for control msg");

			}
		}	
		//omi transmission, do nothing.
		return rxPsd;
	}*/

	/*txAntennaNum[0] = 1;
	txAntennaNum[1] = 1;
	rxAntennaNum[0] = 1;
	rxAntennaNum[1] = 1;*/

	NS_ASSERT_MSG(a->GetDistanceFrom(b) != 0, "the position of tx and rx devices cannot be the same");

	Vector rxSpeed = b->GetVelocity();
	Vector txSpeed = a->GetVelocity();
	Vector relativeSpeed(rxSpeed.x - txSpeed.x, rxSpeed.y - txSpeed.y, rxSpeed.z - txSpeed.z);

	key_t key = std::make_pair(txDevice, rxDevice);
	key_t keyReverse = std::make_pair(rxDevice, txDevice);

	std::map<key_t, Ptr<Params3gpp>>::iterator it = m_channelMap.find(key);
	std::map<key_t, Ptr<Params3gpp>>::iterator itReverse = m_channelMap.find(keyReverse);

	Ptr<Params3gpp> channelParams;

	//180820-jskim14-update analog beamforming vector periodically
	if (txAntennaArray->IsOmniTx() || rxAntennaArray->IsOmniTx()) //control
	{
		if (txEnb != 0 && rxMcUe != 0 && rxUe == 0) //downlink
		{
			if ((m_nrTxMode == 1) && (!m_cellScan))
			{
				//NS_LOG_UNCOND("Omni transmission for control msg");
				if ((it != m_channelMap.end() && it->second->m_channel.size() != 0) || (itReverse != m_channelMap.end() && itReverse->second->m_channel.size() != 0)) //Channel exists
				{
					if (itReverse == m_channelMap.end()) //Forward link
					{
						channelParams = (*it).second;
					}
					else //Reverse link
					{
						channelParams = (*itReverse).second;
					}

					//Check analog beamforming configuration value
					bool configured = channelParams->m_analogBeamSet;
					if (!configured)
					{
						//Change beamforming vector for the channel
						IdealBeamforming(rxPsd, channelParams, txAntennaArray, rxAntennaArray, txAntennaNum, rxAntennaNum);
						NS_LOG_UNCOND(Simulator::Now () << " Analog beamforming vector is updated");
						CalLongTerm(channelParams);
						channelParams->m_analogBeamSet = true;
						m_channelMap[key] = channelParams; //only update forward channel
						//change the analog bemaforming setting parameter to cause the analog beamforming vector to be updated again.
						if (m_analogBeamPeriod.GetMilliSeconds() > 0)
						{
							NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << " schedule analog beamforming vector update for a " << a->GetPosition() << " b " << b->GetPosition());
							Simulator::Schedule(m_analogBeamPeriod, &MmWave3gppChannel::ConfigAnalogBeam, this, a, b);
						}
					}
				}
				else
				{
					//Do nothing
				}
			}
		}
		//omi transmission, do nothing.
		return rxPsd;
	}

	bool reverseLink = false;

	//Step 2: Assign propagation condition (LOS/NLOS).

	char condition;
	if (DynamicCast<MmWave3gppPropagationLossModel>(m_3gppPathloss) != 0)
	{
		condition = m_3gppPathloss->GetObject<MmWave3gppPropagationLossModel>()
						->GetChannelCondition(a->GetObject<MobilityModel>(), b->GetObject<MobilityModel>());
	}
	else if (DynamicCast<MmWave3gppBuildingsPropagationLossModel>(m_3gppPathloss) != 0)
	{
		condition = m_3gppPathloss->GetObject<MmWave3gppBuildingsPropagationLossModel>()
						->GetChannelCondition(a->GetObject<MobilityModel>(), b->GetObject<MobilityModel>());
	}
	else
	{
		NS_FATAL_ERROR("unkonw pathloss model");
	}
	bool los = false;
	bool o2i = false;
	if (condition == 'l')
	{
		los = true;
	}
	else if (condition == 'i')
	{
		o2i = true;
	}
	else if (condition == 's')
	{
		// in this special case, we condiser los + outdoor to indoor.
		los = true;
		o2i = true;
	}

	//Every m_updatePeriod, the channel matrix is deleted and a consistent channel update is triggered.
	//When there is a LOS/NLOS switch, a new uncorrelated channel is created.
	//Therefore, LOS/NLOS condition of updating is always consistent with the previous channel.

	//I only update the fowrad channel.
	if ((it == m_channelMap.end() && itReverse == m_channelMap.end()) ||
		(it != m_channelMap.end() && it->second->m_channel.size() == 0) ||
		(it != m_channelMap.end() && it->second->m_los != los))
	{
		NS_LOG_INFO("Update or create the forward channel");
		NS_LOG_LOGIC("it == m_channelMap.end () " << (it == m_channelMap.end()));
		NS_LOG_LOGIC("itReverse == m_channelMap.end () " << (itReverse == m_channelMap.end()));
		NS_LOG_LOGIC("it->second->m_channel.size() == 0 " << (it->second->m_channel.size() == 0));
		NS_LOG_LOGIC("it->second->m_los != los" << (it->second->m_los != los));

		//Step 1: The parameters are configured in the example code.
		/*make sure txAngle rxAngle exist, i.e., the position of tx and rx cannot be the same*/
		Angles txAngle(b->GetPosition(), a->GetPosition());
		Angles rxAngle(a->GetPosition(), b->GetPosition());
		NS_LOG_UNCOND("Tx angle theta=" << txAngle.theta * 180 / M_PI << ", phi=" << txAngle.phi * 180 / M_PI);
		NS_LOG_UNCOND("Rx angle theta=" << rxAngle.theta * 180 / M_PI << ", phi=" << rxAngle.phi * 180 / M_PI);

		//Step 2: Assign propagation condition (LOS/NLOS).
		//los, o2i condition is computed above.

		//Step 3: The propagation loss is handled in the mmWavePropagationLossModel class.
		double x = a->GetPosition().x - b->GetPosition().x;
		double y = a->GetPosition().y - b->GetPosition().y;
		double distance2D = sqrt(x * x + y * y);
		double hUT, hBS;
		if (rxUe != 0 || rxMcUe != 0)
		{
			hUT = b->GetPosition().z;
			hBS = a->GetPosition().z;
		}
		else
		{
			hUT = a->GetPosition().z;
			hBS = b->GetPosition().z;
		}
		//Draw parameters from table 7.5-6 and 7.5-7 to 7.5-10.
		Ptr<ParamsTable> table3gpp = Get3gppTable(los, o2i, hBS, hUT, distance2D);

		// Step 4-11 are performed in function GetNewChannel()
		if ((it == m_channelMap.end() && itReverse == m_channelMap.end()) ||
			(it != m_channelMap.end() && it->second->m_channel.size() == 0))
		{
			//delete the channel parameter to cause the channel to be updated again.
			//The m_updatePeriod can be configured to be relatively large in order to disable updates.
			if (m_updatePeriod.GetNanoSeconds() > 0)
			{
				NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << " schedule delete for a " << a->GetPosition() << " b " << b->GetPosition());
				Simulator::Schedule(m_updatePeriod, &MmWave3gppChannel::DeleteChannel, this, a, b);
			}
		}

		double distance3D = a->GetDistanceFrom(b);

		bool channelUpdate = false;
		if (it != m_channelMap.end() && it->second->m_channel.size() == 0)
		{
			//if the channel map is not empty, we only update the channel.
			NS_LOG_DEBUG("Update forward channel consistently between device " << a << " " << b);
			it->second->m_locUT = locUT;
			it->second->m_los = los;
			it->second->m_o2i = o2i;
			channelParams = UpdateChannel(it->second, table3gpp, txAntennaArray, rxAntennaArray,
										  txAntennaNum, rxAntennaNum, rxAngle, txAngle);
			it->second->m_dis3D = distance3D;
			it->second->m_dis2D = distance2D;
			it->second->m_speed = relativeSpeed;
			it->second->m_generatedTime = Now();
			it->second->m_preLocUT = locUT;
			channelUpdate = true;
		}
		else
		{
			//if the channel map is empty, we create a new channel.
			NS_LOG_INFO("Create new channel");
			channelParams = GetNewChannel(table3gpp, locUT, los, o2i, txAntennaArray, rxAntennaArray,
										  txAntennaNum, rxAntennaNum, rxAngle, txAngle, relativeSpeed, distance2D, distance3D);
		}
		// the connected pair cannot be trusted anymore! Not initialized at the beginning
		// since the UE may connect at any mmWave eNB
		// we can look for if the eNB is the target eNB in the UE
		bool connectedPair = false;
		if (downlink)
		{
			Ptr<MmWaveEnbNetDevice> enbTx = DynamicCast<MmWaveEnbNetDevice>(txDevice);
			Ptr<MmWaveUeNetDevice> ueRx = DynamicCast<MmWaveUeNetDevice>(rxDevice);
			if (enbTx == ueRx->GetTargetEnb())
			{
				connectedPair = true;
			}
		}
		else if (downlinkMc)
		{
			Ptr<MmWaveEnbNetDevice> enbTx = DynamicCast<MmWaveEnbNetDevice>(txDevice);
			Ptr<McUeNetDevice> ueRx = DynamicCast<McUeNetDevice>(rxDevice);
			if (enbTx == ueRx->GetMmWaveTargetEnb())
			{
				connectedPair = true;
			}
		}
		else if (uplink)
		{
			Ptr<MmWaveUeNetDevice> ueTx = DynamicCast<MmWaveUeNetDevice>(txDevice);
			Ptr<MmWaveEnbNetDevice> enbRx = DynamicCast<MmWaveEnbNetDevice>(rxDevice);
			if (enbRx == ueTx->GetTargetEnb())
			{
				connectedPair = true;
			}
		}
		else if (uplinkMc)
		{
			Ptr<McUeNetDevice> ueTx = DynamicCast<McUeNetDevice>(txDevice);
			Ptr<MmWaveEnbNetDevice> enbRx = DynamicCast<MmWaveEnbNetDevice>(rxDevice);
			if (enbRx == ueTx->GetMmWaveTargetEnb())
			{
				connectedPair = true;
			}
		}

		//std::map< key_t, int >::iterator it1 = m_connectedPair.find (key);
		if (connectedPair || m_forceInitialBfComputation || channelUpdate)
		// this is true for connected devices at each transmission,
		// and for non-connected devices at each channel update or at the beginning of the simulation
		{
			NS_LOG_INFO("connectedPair " << connectedPair << " m_forceInitialBfComputation " << m_forceInitialBfComputation << " channelUpdate " << channelUpdate);
			//180629-jskim14-add NR tx mode
			if (m_nrTxMode == 1)
			{
				if (m_cellScan)
				{
					IdealBeamforming(rxPsd, channelParams, txAntennaArray, rxAntennaArray, txAntennaNum, rxAntennaNum);
				}
				else
				{
					//180813-jskim14-apply beamforming
					if (m_forceInitialBfComputation)
					{
						//For initial beamforming vector setting
						NS_LOG_UNCOND("Inital analog beamforming vector setting");
						IdealBeamforming(rxPsd, channelParams, txAntennaArray, rxAntennaArray, txAntennaNum, rxAntennaNum);
						channelParams->m_analogBeamSet = true; //set analog bemaforming vector
						if (m_analogBeamPeriod.GetMilliSeconds() > 0)
						{
							NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << " schedule analog beamforming vector update for a " << a->GetPosition() << " b " << b->GetPosition());
							Simulator::Schedule(m_analogBeamPeriod, &MmWave3gppChannel::ConfigAnalogBeam, this, a, b);
						}
					}
					else
					{
						//Do nothing
						NS_LOG_UNCOND("Fast fading channel is updated");
					}
				}
			}
			else
			{
				//jskim14-end
				if (m_cellScan)
				{
					BeamSearchBeamforming(rxPsd, channelParams, txAntennaArray, rxAntennaArray, txAntennaNum, rxAntennaNum);
				}
				else
				{
					LongTermCovMatrixBeamforming(channelParams);
				}
			}
			txAntennaArray->SetBeamformingVector(channelParams->m_txW, rxDevice);
			rxAntennaArray->SetBeamformingVector(channelParams->m_rxW, txDevice);
		}
		else
		{
			NS_LOG_INFO("Not a connected pair");
			channelParams->m_txW = txAntennaArray->GetBeamformingVector();
			channelParams->m_rxW = rxAntennaArray->GetBeamformingVector();
			if (channelParams->m_txW.size() == 0 || channelParams->m_rxW.size() == 0)
			{
				NS_LOG_INFO("channelParams->m_txW.size() == 0 " << (channelParams->m_txW.size() == 0));
				NS_LOG_INFO("channelParams->m_rxW.size() == 0 " << (channelParams->m_rxW.size() == 0));
				m_channelMap[key] = channelParams;
				return rxPsd;
			}
		}

		CalLongTerm(channelParams);
		m_channelMap[key] = channelParams;
	}
	else if (itReverse == m_channelMap.end()) //Find channel matrix in the forward link
	{
		channelParams = (*it).second;
	}
	else //Find channel matrix in the Reverse link
	{
		reverseLink = true;
		channelParams = (*itReverse).second;
	}

	Ptr<SpectrumValue> bfPsd = CalBeamformingGain(rxPsd, channelParams, relativeSpeed);

	SpectrumValue bfGain = (*bfPsd) / (*rxPsd);
	uint8_t nbands = bfGain.GetSpectrumModel()->GetNumBands();
	double avgRxPsd = Sum(*rxPsd) / nbands;
	if (reverseLink == false)
	{
		NS_LOG_UNCOND("****** DL BF gain == " << 10 * std::log10(Sum(bfGain) / nbands) << ", RX PSD[dB]=" << 10 * log10(avgRxPsd)); // print avg bf gain
	}
	else
	{
		NS_LOG_DEBUG("****** UL BF gain == " << 10 * std::log10(Sum(bfGain) / nbands) << " RX PSD " << Sum(*rxPsd) / nbands);
	}
	return bfPsd;
}

void MmWave3gppChannel::LongTermCovMatrixBeamforming(Ptr<Params3gpp> params) const
{
	//generate transmitter side spatial correlation matrix
	uint8_t txSize = params->m_channel.at(0).size();
	uint8_t rxSize = params->m_channel.size();
	complex2DVector_t txQ;
	txQ.resize(txSize);

	for (uint8_t txIndex = 0; txIndex < txSize; txIndex++)
	{
		txQ.at(txIndex).resize(txSize);
	}

	//compute the transmitter side spatial correlation matrix txQ = H*H, where H is the sum of H_n over n clusters.
	for (uint8_t t1Index = 0; t1Index < txSize; t1Index++)
	{
		for (uint8_t t2Index = 0; t2Index < txSize; t2Index++)
		{
			for (uint8_t rxIndex = 0; rxIndex < rxSize; rxIndex++)
			{
				std::complex<double> cSum(0, 0);
				for (uint8_t cIndex = 0; cIndex < params->m_channel.at(rxIndex).at(t1Index).size(); cIndex++)
				{
					cSum = cSum + std::conj(params->m_channel.at(rxIndex).at(t1Index).at(cIndex)) *
									  (params->m_channel.at(rxIndex).at(t2Index).at(cIndex));
				}
				txQ[t1Index][t2Index] += cSum;
			}
		}
	}

	//calculate beamforming vector from spatial correlation matrix.
	complexVector_t antennaWeights;
	uint8_t txAntenna = txQ.size();
	for (uint8_t eIndex = 0; eIndex < txAntenna; eIndex++)
	{
		antennaWeights.push_back(txQ.at(0).at(eIndex));
	}

	int iter = 10;
	double diff = 1;
	while (iter != 0 && diff > 1e-10)
	{
		complexVector_t antennaWeights_New;

		for (uint8_t row = 0; row < txAntenna; row++)
		{
			std::complex<double> sum(0, 0);
			for (uint8_t col = 0; col < txAntenna; col++)
			{
				sum += txQ.at(row).at(col) * antennaWeights.at(col);
			}

			antennaWeights_New.push_back(sum);
		}
		//normalize antennaWeights;
		double weightSum = 0;
		for (uint8_t i = 0; i < txAntenna; i++)
		{
			weightSum += norm(antennaWeights_New.at(i));
		}
		for (uint8_t i = 0; i < txAntenna; i++)
		{
			antennaWeights_New.at(i) = antennaWeights_New.at(i) / sqrt(weightSum);
		}
		diff = 0;
		for (uint8_t i = 0; i < txAntenna; i++)
		{
			diff += std::norm(antennaWeights_New.at(i) - antennaWeights.at(i));
		}
		iter--;
		antennaWeights = antennaWeights_New;
	}

	params->m_txW = antennaWeights;

	//compute the receiver side spatial correlation matrix rxQ = HH*, where H is the sum of H_n over n clusters.
	complex2DVector_t rxQ;
	rxQ.resize(rxSize);
	for (uint8_t r1Index = 0; r1Index < rxSize; r1Index++)
	{
		rxQ.at(r1Index).resize(rxSize);
	}

	for (uint8_t r1Index = 0; r1Index < rxSize; r1Index++)
	{
		for (uint8_t r2Index = 0; r2Index < rxSize; r2Index++)
		{
			for (uint8_t txIndex = 0; txIndex < txSize; txIndex++)
			{
				std::complex<double> cSum(0, 0);
				for (uint8_t cIndex = 0; cIndex < params->m_channel.at(r1Index).at(txIndex).size(); cIndex++)
				{
					cSum = cSum + params->m_channel.at(r1Index).at(txIndex).at(cIndex) *
									  std::conj(params->m_channel.at(r2Index).at(txIndex).at(cIndex));
				}
				rxQ[r1Index][r2Index] += cSum;
			}
		}
	}

	//calculate beamforming vector from spatial correlation matrix.
	antennaWeights.clear();
	uint8_t rxAntenna = rxQ.size();
	for (uint8_t eIndex = 0; eIndex < rxAntenna; eIndex++)
	{
		antennaWeights.push_back(rxQ.at(0).at(eIndex));
	}

	iter = 10;
	diff = 1;
	while (iter != 0 && diff > 1e-10)
	{
		complexVector_t antennaWeights_New;

		for (uint8_t row = 0; row < rxAntenna; row++)
		{
			std::complex<double> sum(0, 0);
			for (uint8_t col = 0; col < rxAntenna; col++)
			{
				sum += rxQ.at(row).at(col) * antennaWeights.at(col);
			}

			antennaWeights_New.push_back(sum);
		}

		//normalize antennaWeights;
		double weightSum = 0;
		for (uint8_t i = 0; i < rxAntenna; i++)
		{
			weightSum += norm(antennaWeights_New.at(i));
		}
		for (uint8_t i = 0; i < rxAntenna; i++)
		{
			antennaWeights_New.at(i) = antennaWeights_New.at(i) / sqrt(weightSum);
		}
		diff = 0;
		for (uint8_t i = 0; i < rxAntenna; i++)
		{
			diff += std::norm(antennaWeights_New.at(i) - antennaWeights.at(i));
		}
		iter--;
		antennaWeights = antennaWeights_New;
	}

	params->m_rxW = antennaWeights;
}

Ptr<SpectrumValue>
MmWave3gppChannel::CalBeamformingGain(Ptr<const SpectrumValue> txPsd, Ptr<Params3gpp> params, Vector speed) const
{
	NS_LOG_FUNCTION(this);

	Ptr<SpectrumValue> tempPsd = Copy<SpectrumValue>(txPsd);

	//180714-jskim14
	Vector relativeSpeed = speed;
	NS_LOG_INFO("Relative speed=" << relativeSpeed.x << ", " << relativeSpeed.y << ", " << relativeSpeed.z);
	Angles velocity_angle = Angles(relativeSpeed);
	NS_LOG_INFO("Velocity angle: theta=" << velocity_angle.theta * 180 / M_PI << ", phi=" << velocity_angle.phi * 180 / M_PI);
	//jskim14-end

	//NS_ASSERT_MSG (params->m_delay.size()==params->m_channel.at(0).at(0).size(), "the cluster number of channel and delay spread should be the same");
	//NS_ASSERT_MSG (params->m_txW.size()==params->m_channel.at(0).size(), "the tx antenna size of channel and antenna weights should be the same");
	//NS_ASSERT_MSG (params->m_rxW.size()==params->m_channel.size(), "the rx antenna size of channel and antenna weights should be the same");
	//NS_ASSERT_MSG (params->m_angle.at(0).size()==params->m_channel.at(0).at(0).size(), "the cluster number of channel and AOA should be the same");
	//NS_ASSERT_MSG (params->m_angle.at(1).size()==params->m_channel.at(0).at(0).size(), "the cluster number of channel and ZOA should be the same");

	//channel[rx][tx][cluster]
	uint8_t numCluster = params->m_delay.size();
	//uint8_t txAntenna = params->m_txW.size();
	//uint8_t rxAntenna = params->m_rxW.size();
	//the update of Doppler is simplified by only taking the center angle of each cluster in to consideration.
	Values::iterator vit = tempPsd->ValuesBegin();
	uint16_t iSubband = 0;
	double slotTime = Simulator::Now().GetSeconds();
	complexVector_t doppler;
	for (uint8_t cIndex = 0; cIndex < numCluster; cIndex++)
	{
		double temp_doppler;
		if (relativeSpeed.GetLength() == 0) //this means relative speed is 0
		{
			temp_doppler = 0;
		}
		else
		{
			//cluster angle angle[direction][n],where, direction = 0(aoa), 1(zoa).
			temp_doppler = 2 * M_PI * (sin(params->m_angle.at(ZOA_INDEX).at(cIndex) * M_PI / 180) * cos(params->m_angle.at(AOA_INDEX).at(cIndex) * M_PI / 180) * speed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) + sin(params->m_angle.at(ZOA_INDEX).at(cIndex) * M_PI / 180) * sin(params->m_angle.at(AOA_INDEX).at(cIndex) * M_PI / 180) * speed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) + cos(params->m_angle.at(ZOA_INDEX).at(cIndex) * M_PI / 180) * speed.z * cos(velocity_angle.theta)) * slotTime * m_phyMacConfig->GetCenterFrequency() / 3e8;
		}
		doppler.push_back(exp(std::complex<double>(0, temp_doppler)));
	}

	while (vit != tempPsd->ValuesEnd())
	{
		std::complex<double> subsbandGain(0.0, 0.0);
		if ((*vit) != 0.00)
		{
			double fsb = m_phyMacConfig->GetCenterFrequency() - GetSystemBandwidth() / 2 + m_phyMacConfig->GetChunkWidth() * iSubband;
			for (uint8_t cIndex = 0; cIndex < numCluster; cIndex++)
			{
				double delay = -2 * M_PI * fsb * (params->m_delay.at(cIndex));
				std::complex<double> txSum(0, 0);

				subsbandGain = subsbandGain + params->m_longTerm.at(cIndex) * exp(std::complex<double>(0, delay)) * doppler.at(cIndex);
			}
			*vit = (*vit) * (norm(subsbandGain));
		}
		vit++;
		iSubband++;
	}
	return tempPsd;
}

double
MmWave3gppChannel::GetSystemBandwidth() const
{
	double bw = 0.00;
	bw = m_phyMacConfig->GetChunkWidth() * m_phyMacConfig->GetNumChunkPerRb() * m_phyMacConfig->GetNumRb();
	return bw;
}

void MmWave3gppChannel::SetPathlossModel(Ptr<PropagationLossModel> pathloss)
{
	m_3gppPathloss = pathloss;
	if (DynamicCast<MmWave3gppPropagationLossModel>(m_3gppPathloss) != 0)
	{
		m_scenario = m_3gppPathloss->GetObject<MmWave3gppPropagationLossModel>()->GetScenario();
	}
	else if (DynamicCast<MmWave3gppBuildingsPropagationLossModel>(m_3gppPathloss) != 0)
	{
		m_scenario = m_3gppPathloss->GetObject<MmWave3gppBuildingsPropagationLossModel>()->GetScenario();
	}
	else
	{
		NS_FATAL_ERROR("unkonw pathloss model");
	}
}

// 180628-jskim14, set NR tx mode in 3gpp channel
void MmWave3gppChannel::SetNrTxMode(uint8_t nrTxMode)
{
	m_nrTxMode = nrTxMode;
}
// jskim14-end

void MmWave3gppChannel::CalLongTerm(Ptr<Params3gpp> params) const
{
	//180709-jskim14-consider NR tx mode
	uint16_t txAntenna;
	uint16_t rxAntenna;
	if (m_nrTxMode == 1)
	{
		txAntenna = params->m_txWMat.size(); //actually, this is vector.
		rxAntenna = params->m_rxWMat.size(); //actually, this is vector.
	}
	else
	{
		txAntenna = params->m_txW.size();
		rxAntenna = params->m_rxW.size();
	}
	//jskim14-end

	//store the long term part to reduce computation load
	//only the small scale fading is need to be updated if the large scale parameters and antenna weights remain unchanged.
	complexVector_t longTerm;
	uint8_t numCluster = params->m_delay.size();

	//NS_LOG_UNCOND("channel " << params->m_channel.size() << " " << params->m_channel[0].size() << " " << params->m_channel[0][0].size()) ;

	for (uint8_t cIndex = 0; cIndex < numCluster; cIndex++)
	{
		std::complex<double> txSum(0, 0);
		for (uint8_t txIndex = 0; txIndex < txAntenna; txIndex++)
		{
			std::complex<double> rxSum(0, 0);
			for (uint8_t rxIndex = 0; rxIndex < rxAntenna; rxIndex++)
			{
				//180709-jskim14-consider NR tx mode
				if (m_nrTxMode == 1)
				{
					rxSum = rxSum + std::conj(params->m_rxWMat[rxIndex][0]) * params->m_channel.at(rxIndex).at(txIndex).at(cIndex);
					//NS_LOG_UNCOND("RX beamvector[" << (unsigned)rxIndex << "]=" << params->m_rxWMat[rxIndex][0] << "Channel[" << (unsigned)rxIndex << "][" << (unsigned)txIndex << "][" << (unsigned)cIndex << "]=" << params->m_channel.at(rxIndex).at(txIndex).at(cIndex));
				}
				else
				//jskim14-end
				{
					rxSum = rxSum + std::conj(params->m_rxW.at(rxIndex)) * params->m_channel.at(rxIndex).at(txIndex).at(cIndex);
				}
			}

			//180709-jskim14-consider NR tx mode
			if (m_nrTxMode == 1)
			{
				txSum = txSum + params->m_txWMat[txIndex][0] * rxSum;
			}
			else
			//jskim14-end
			{
				txSum = txSum + params->m_txW.at(txIndex) * rxSum;
			}
		}
		longTerm.push_back(txSum);
		//NS_LOG_UNCOND("Cluster power[" << (unsigned)cIndex << "]=" << txSum);
	}
	params->m_longTerm = longTerm;
}

Ptr<ParamsTable>
MmWave3gppChannel::Get3gppTable(bool los, bool o2i, double hBS, double hUT, double distance2D) const
{
	//For cacluating uLgZSD, we use max function. 2018.05.31 shlim
	//los/nlos maxuLgZSD is defined at TR 38.901, Table 7.5-9

	double fcGHz = m_phyMacConfig->GetCenterFrequency() / 1e9;

	// Both o2i and nlos have same maxuLgZSD value.

	Ptr<ParamsTable> table3gpp = CreateObject<ParamsTable>();
	// table3gpp includes the following parameters:
	// numOfCluster, raysPerCluster, uLgDS, sigLgDS, uLgASD, sigLgASD,
	// uLgASA, sigLgASA, uLgZSA, sigLgZSA, uLgZSD, sigLgZSD, offsetZOD,
	// cDS, cASD, cASA, cZSA, uK, sigK, rTau, shadowingStd

	//In NLOS case, parameter uK and sigK are not used and 0 is passed into the SetParams() function.
	//2018.07.03 shlim. For all scenario, we insert the xpr value to implement step 9.
	if (m_scenario == "RMa")
	{
		double maxuLgZSD_los = -0.17 * distance2D / 1000 - 0.01 * (hUT - 1.5) + 0.22;
		if (maxuLgZSD_los < -1)
		{
			maxuLgZSD_los = -1;
		}
		double maxuLgZSD_o2i_nlos = -0.19 * distance2D / 1000 - 0.01 * (hUT - 1.5) + 0.28;
		if (maxuLgZSD_o2i_nlos < -1)
		{
			maxuLgZSD_o2i_nlos = -1;
		}

		//For RMa, the outdoor LOS/NLOS and o2i LOS/NLOS is the same. -> ??
		if (los)
		{
			//3GPP mentioned that 3.91 ns should be used when the Cluster DS (cDS) entry is N/A.
			//uLgZSA, sigLgZSA, ulgZSD, sigLgZSD, shadowingstd are changed according to TR 38.901. 2018.05.31 shlim

			table3gpp->SetParams(11, 20, -7.49, 0.55, 0.90, 0.38, 1.52, 0.24, 0.47, 0.40,
								 maxuLgZSD_los, 0.34, 0, 3.91e-9, 2, 3, 3, 7, 4, 3.8, 3, 12, 4);
			for (uint8_t row = 0; row < 7; row++)
			{
				for (uint8_t column = 0; column < 7; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_RMa_LOS[row][column];
				}
			}
		}
		else if (o2i)
		{
			//o2i case
			//offsetZod is also changed according to TR 38.901 - Table 7.5-9. 2018.06.04 shlim
			double offsetZod = atan((35 - 3.5) / distance2D) - atan((35 - 1.5) / distance2D);

			table3gpp->SetParams(10, 20, -7.47, 0.24, 0.67, 0.18, 1.66, 0.21, 0.93, 0.22,
								 maxuLgZSD_o2i_nlos, 0.30, offsetZod, 3.91e-9, 2, 3, 3, 0, 0, 1.7, 3, 7, 3);
			for (uint8_t row = 0; row < 6; row++)
			{
				for (uint8_t column = 0; column < 6; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_RMa_O2I[row][column];
				}
			}
		}
		else
		{
			double offsetZod = atan((35 - 3.5) / distance2D) - atan((35 - 1.5) / distance2D); // 180628-jskim14-fix error
			table3gpp->SetParams(10, 20, -7.43, 0.48, 0.95, 0.45, 1.52, 0.13, 0.88, 0.16,
								 0.3, 0.49, offsetZod, 3.91e-9, 2, 3, 3, 0, 0, 1.7, 3, 7, 3);
			for (uint8_t row = 0; row < 6; row++)
			{
				for (uint8_t column = 0; column < 6; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_RMa_NLOS[row][column];
				}
			}
		}
	}

	else if (m_scenario == "UMa")
	{
		if (los && !o2i)
		{
			double uLgZSD_los = std::max(-0.5, -2.1 * distance2D / 1000 - 0.01 * (hUT - 1.5) + 0.75);
			double cDs = std::max(0.25, -3.4084 * log10(fcGHz) + 6.5622) * 1e-9;
			table3gpp->SetParams(12, 20, -6.955 - 0.0963 * log10(fcGHz), 0.66, 1.06 + 0.1114 * log10(fcGHz),
								 0.28, 1.81, 0.20, 0.95, 0.16, uLgZSD_los, 0.40, 0, cDs, 5, 11, 7, 9, 3.5, 2.5, 3, 8, 4);
			for (uint8_t row = 0; row < 7; row++)
			{
				for (uint8_t column = 0; column < 7; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_UMa_LOS[row][column];
				}
			}
		}
		else
		{
			double uLgZSD_nlos = std::max(-0.5, -2.1 * distance2D / 1000 - 0.01 * (hUT - 1.5) + 0.9);

			double afc = 0.208 * log10(fcGHz) - 0.782;
			double bfc = 25;
			double cfc = -0.13 * log10(fcGHz) + 2.03;
			double efc = 7.66 * log10(fcGHz) - 5.96;

			double offsetZOD = efc - std::pow(10, afc * log10(std::max(bfc, distance2D)) + cfc - 0.07 * (hUT - 1.5));
			// offsetZod is changed according to TR 38.901 - Table 7.5-7. 2018.06.05 shlim
			double cDS = std::max(0.25, -3.4084 * log10(fcGHz) + 6.5622) * 1e-9;

			if (!los && !o2i) //nlos
			{
				table3gpp->SetParams(20, 20, -6.28 - 0.204 * log10(fcGHz), 0.39, 1.5 - 0.1144 * log10(fcGHz),
									 0.28, 2.08 - 0.27 * log10(fcGHz), 0.11, -0.3236 * log10(fcGHz) + 1.512, 0.16, uLgZSD_nlos,
									 0.49, offsetZOD, cDS, 2, 15, 7, 0, 0, 2.2, 3, 7, 3);
				// Delay scaling parameter(rTau) is changed according to TR 38.901 - Table 7.5-6. 2018.06.05 shlim

				for (uint8_t row = 0; row < 6; row++)
				{
					for (uint8_t column = 0; column < 6; column++)
					{
						table3gpp->m_sqrtC[row][column] = sqrtC_UMa_NLOS[row][column];
					}
				}
			}
			else if (los && o2i) //(los_o2i)
								 //o2i is separated to los_o2i and nlos_o2i according to TR 38.901 - Notes for Table 7.5-7~7.5-8 - 2018.06.05 shlim
			{
				double uLgZSD_los = std::max(-0.5, -2.1 * distance2D / 1000 - 0.01 * (hUT - 1.5) + 0.75); // 180628-jskim14-fix error
				table3gpp->SetParams(12, 20, -6.62, 0.32, 1.25, 0.42, 1.76, 0.16, 1.01, 0.43,
									 uLgZSD_los, 0.40, 0, 11e-9, 5, 8, 3, 0, 0, 2.2, 4, 9, 5);
				// Cluster ASA and Cluster ZSA are changed according to TR 38.901 - Table 7.5-6. 2018.06.05 shlim
				for (uint8_t row = 0; row < 6; row++)
				{
					for (uint8_t column = 0; column < 6; column++)
					{
						table3gpp->m_sqrtC[row][column] = sqrtC_UMa_O2I[row][column];
					}
				}
			}
			else //(nlos_o2i)
			{
				table3gpp->SetParams(12, 20, -6.62, 0.32, 1.25, 0.42, 1.76, 0.16, 1.01, 0.43,
									 uLgZSD_nlos, 0.49, offsetZOD, 11e-9, 5, 8, 3, 0, 0, 2.2, 4, 9, 5);
				for (uint8_t row = 0; row < 6; row++)
				{
					for (uint8_t column = 0; column < 6; column++)
					{
						table3gpp->m_sqrtC[row][column] = sqrtC_UMa_O2I[row][column];
					}
				}
			}
		}
	}
	else if (m_scenario == "UMi-StreetCanyon")
	{
		if (los && !o2i)
		{
			double uLgZSD = std::max(-0.21, -14.8 * distance2D / 1000 + 0.01 * std::abs(hUT - hBS) + 0.83);
			table3gpp->SetParams(12, 20, -0.24 * log10(1 + fcGHz) - 7.14, 0.38, -0.05 * log10(1 + fcGHz) + 1.21, 0.41,
								 -0.08 * log10(1 + fcGHz) + 1.73, 0.014 * log10(1 + fcGHz) + 0.28, -0.1 * log10(1 + fcGHz) + 0.73, -0.04 * log10(1 + fcGHz) + 0.34,
								 uLgZSD, 0.35, 0, 5e-9, 3, 17, 7, 9, 5, 3, 3, 9, 3);
			for (uint8_t row = 0; row < 7; row++)
			{
				for (uint8_t column = 0; column < 7; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_UMi_LOS[row][column];
				}
			}
		}
		else // There's no uLgZSD of o2i at TR 38.901, Table 7.5-8.
		{
			double uLgZSD = std::max(-0.5, -3.1 * distance2D / 1000 + 0.01 * std::max(hUT - hBS, 0.0) + 0.2);
			double offsetZOD = -1 * std::pow(10, -1.5 * log10(std::max(10.0, distance2D)) + 3.3);
			if (!los && !o2i) //(nlos)
			{
				table3gpp->SetParams(19, 20, -0.24 * log10(1 + fcGHz) - 6.83, 0.16 * log10(1 + fcGHz) + 0.28, -0.23 * log10(1 + fcGHz) + 1.53,
									 0.11 * log10(1 + fcGHz) + 0.33, -0.08 * log10(1 + fcGHz) + 1.81, 0.05 * log10(1 + fcGHz) + 0.3,
									 -0.04 * log10(1 + fcGHz) + 0.92, -0.07 * log10(1 + fcGHz) + 0.41, uLgZSD, 0.35, offsetZOD,
									 11e-9, 10, 22, 7, 0, 0, 2.1, 3, 8, 3);
				for (uint8_t row = 0; row < 6; row++)
				{
					for (uint8_t column = 0; column < 6; column++)
					{
						table3gpp->m_sqrtC[row][column] = sqrtC_UMi_NLOS[row][column];
					}
				}
			}
			else //(o2i)
			{
				table3gpp->SetParams(12, 20, -6.62, 0.32, 1.25, 0.42, 1.76, 0.16, 1.01, 0.43,
									 uLgZSD, 0.35, offsetZOD, 11e-9, 5, 8, 3, 0, 0, 2.2, 4, 9, 5);
				// Cluster ASA and Cluster ZSA are changed according to TR 38.901 - table 7.5-6. 2018.06.18 shlim
				for (uint8_t row = 0; row < 6; row++)
				{
					for (uint8_t column = 0; column < 6; column++)
					{
						table3gpp->m_sqrtC[row][column] = sqrtC_UMi_O2I[row][column];
					}
				}
			}
		}
	}
	else if (m_scenario == "InH-OfficeMixed" || m_scenario == "InH-OfficeOpen")
	{
		NS_ASSERT_MSG(!o2i, "The indoor scenario does out support outdoor to indoor");
		if (los)
		{
			table3gpp->SetParams(15, 20, -0.01 * log10(1 + fcGHz) - 7.692, 0.18, 1.60, 0.18,
								 -0.19 * log10(1 + fcGHz) + 1.781, 0.12 * log10(1 + fcGHz) + 0.119, -0.26 * log10(1 + fcGHz) + 1.44, -0.04 * log10(1 + fcGHz) + 0.264,
								 -1.43 * log10(1 + fcGHz) + 2.228, 0.13 * log10(1 + fcGHz) + 0.30, 0, 3.91e-9, 5, 8,
								 9, 7, 4, 3.6, 6, 11, 4);
			// Nuber of clusters, uLgDS, uLgASA, sigLgASA, uLgZSA, sigLgZSA, uLgZSD,
			// sigLgZSD, cASD, cASA, cZSA, uK, sigK, rTau are changed according to TR 38.901 - table 7.5-6. 2018.06.21 shlim
			for (uint8_t row = 0; row < 7; row++)
			{
				for (uint8_t column = 0; column < 7; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_office_LOS[row][column];
				}
			}
		}
		else
		{
			table3gpp->SetParams(19, 20, -0.28 * log10(1 + fcGHz) - 7.173, 0.1 * log10(1 + fcGHz) + 0.055, 1.62, 0.25,
								 -0.11 * log10(1 + fcGHz) + 1.863, 0.12 * log10(1 + fcGHz) + 0.059, -0.15 * log10(1 + fcGHz) + 1.387, -0.09 * log10(1 + fcGHz) + 0.746,
								 1.08, 0.36, 0, 3.91e-9, 5, 11, 9, 0, 0, 3, 3, 10, 4);
			// Nuber of clusters, uLgDS, sigLgDS, uLgASD, sigLgASD, uLgASA, sigLgASA, uLgZSA, sigLgZSA, uLgZSD,
			// sigLgZSD, cASD, cASA, cZSA, rTau are changed according to TR 38.901 - table 7.5-6. 2018.06.21 shlim
			for (uint8_t row = 0; row < 6; row++)
			{
				for (uint8_t column = 0; column < 6; column++)
				{
					table3gpp->m_sqrtC[row][column] = sqrtC_office_NLOS[row][column];
				}
			}
		}
	}
	else
	{
		//Note that the InH-ShoppingMall scenario is not given in the table 7.5-6
		NS_FATAL_ERROR("unkonw scenarios");
	}

	return table3gpp;
}

void MmWave3gppChannel::DeleteChannel(Ptr<const MobilityModel> a, Ptr<const MobilityModel> b) const
{
	Ptr<NetDevice> dev1 = a->GetObject<Node>()->GetDevice(0);
	Ptr<NetDevice> dev2 = b->GetObject<Node>()->GetDevice(0);
	NS_LOG_INFO("a position " << a->GetPosition() << " b " << b->GetPosition());
	Ptr<Params3gpp> params = m_channelMap.find(std::make_pair(dev1, dev2))->second;
	NS_LOG_INFO("params " << params);
	NS_LOG_INFO("params m_channel size" << params->m_channel.size());
	NS_ASSERT_MSG(m_channelMap.find(std::make_pair(dev1, dev2)) != m_channelMap.end(), "Channel not found");
	params->m_channel.clear();
	m_channelMap[std::make_pair(dev1, dev2)] = params;
}

//180821-jskim14-analog beamforming vector setting
void MmWave3gppChannel::ConfigAnalogBeam(Ptr<const MobilityModel> a, Ptr<const MobilityModel> b) const
{
	Ptr<NetDevice> dev1 = a->GetObject<Node>()->GetDevice(0);
	Ptr<NetDevice> dev2 = b->GetObject<Node>()->GetDevice(0);
	NS_LOG_INFO("a position " << a->GetPosition() << " b " << b->GetPosition());
	Ptr<Params3gpp> params = m_channelMap.find(std::make_pair(dev1, dev2))->second;
	NS_LOG_INFO("params m_channel size" << params->m_channel.size());
	NS_ASSERT_MSG(m_channelMap.find(std::make_pair(dev1, dev2)) != m_channelMap.end(), "Channel not found");
	params->m_analogBeamSet = false;
	m_channelMap[std::make_pair(dev1, dev2)] = params;
}
//jskim14-end

Ptr<Params3gpp>
MmWave3gppChannel::GetNewChannel(Ptr<ParamsTable> table3gpp, Vector locUT, bool los, bool o2i,
								 Ptr<AntennaArrayModel> txAntenna, Ptr<AntennaArrayModel> rxAntenna,
								 uint8_t *txAntennaNum, uint8_t *rxAntennaNum, Angles &rxAngle, Angles &txAngle,
								 Vector speed, double dis2D, double dis3D) const
{
	// We initialize slotTime to 0 because this function is 'Get New Channel.' 2018.07.11 shlim.
	uint8_t numOfCluster = table3gpp->m_numOfCluster;
	uint8_t raysPerCluster = table3gpp->m_raysPerCluster;
	Ptr<Params3gpp> channelParams = Create<Params3gpp>();
	//for new channel, the previous and current location is the same.
	channelParams->m_preLocUT = locUT;
	channelParams->m_locUT = locUT;
	channelParams->m_los = los;
	channelParams->m_o2i = o2i;
	channelParams->m_generatedTime = Now();
	channelParams->m_speed = speed;
	channelParams->m_dis2D = dis2D;
	channelParams->m_dis3D = dis3D;
	//Step 4: Generate large scale parameters. All LSPS are uncorrelated.
	doubleVector_t LSPsIndep, LSPs;
	uint8_t paramNum;
	if (los)
	{
		paramNum = 7;
	}
	else
	{
		paramNum = 6;
	}
	//Generate paramNum independent LSPs.
	for (uint8_t iter = 0; iter < paramNum; iter++)
	{
		LSPsIndep.push_back(m_normalRv->GetValue());
	}
	for (uint8_t row = 0; row < paramNum; row++)
	{
		double temp = 0;
		for (uint8_t column = 0; column < paramNum; column++)
		{
			temp += table3gpp->m_sqrtC[row][column] * LSPsIndep.at(column);
		}
		LSPs.push_back(temp);
	}

	/*std::cout << "LSPsIndep:";
	for (uint8_t i = 0; i < paramNum; i++)
	{
		std::cout <<LSPsIndep.at(i)<<"\t";
	}
	std::cout << "\n";
	std::cout << "LSPs:";
	for (uint8_t i = 0; i < paramNum; i++)
	{
		std::cout <<LSPs.at(i)<<"\t";
	}
	std::cout << "\n";*/

	/* Notice the shadowing is updated much frequently (every transmission),
	 * therefore it is generated separately in the 3GPP propagation loss model.*/

	double DS, ASD, ASA, ZSA, ZSD, K_factor = 0;
	if (los)
	{
		K_factor = LSPs.at(1) * table3gpp->m_sigK + table3gpp->m_uK;
		DS = pow(10, LSPs.at(2) * table3gpp->m_sigLgDS + table3gpp->m_uLgDS);
		ASD = pow(10, LSPs.at(3) * table3gpp->m_sigLgASD + table3gpp->m_uLgASD);
		ASA = pow(10, LSPs.at(4) * table3gpp->m_sigLgASA + table3gpp->m_uLgASA);
		ZSD = pow(10, LSPs.at(5) * table3gpp->m_sigLgZSD + table3gpp->m_uLgZSD);
		ZSA = pow(10, LSPs.at(6) * table3gpp->m_sigLgZSA + table3gpp->m_uLgZSA);
	}
	else
	{
		DS = pow(10, LSPs.at(1) * table3gpp->m_sigLgDS + table3gpp->m_uLgDS);
		ASD = pow(10, LSPs.at(2) * table3gpp->m_sigLgASD + table3gpp->m_uLgASD);
		ASA = pow(10, LSPs.at(3) * table3gpp->m_sigLgASA + table3gpp->m_uLgASA);
		ZSD = pow(10, LSPs.at(4) * table3gpp->m_sigLgZSD + table3gpp->m_uLgZSD);
		ZSA = pow(10, LSPs.at(5) * table3gpp->m_sigLgZSA + table3gpp->m_uLgZSA);
	}
	ASD = std::min(ASD, 104.0);
	ASA = std::min(ASA, 104.0);
	ZSD = std::min(ZSD, 52.0);
	ZSA = std::min(ZSA, 52.0);

	channelParams->m_DS = DS;
	channelParams->m_K = K_factor;

	NS_LOG_INFO("K-factor=" << K_factor << ",DS=" << DS << ", ASD=" << ASD << ", ASA=" << ASA << ", ZSD=" << ZSD << ", ZSA=" << ZSA);

	//Step 5: Generate Delays.
	doubleVector_t clusterDelay;
	double minTau = 100.0;
	for (uint8_t cIndex = 0; cIndex < numOfCluster; cIndex++)
	{
		double tau = -1 * table3gpp->m_rTau * DS * log(m_uniformRv->GetValue(0, 1)); //(7.5-1)
		if (minTau > tau)
		{
			minTau = tau;
		}
		clusterDelay.push_back(tau);
	}

	for (uint8_t cIndex = 0; cIndex < numOfCluster; cIndex++)
	{
		clusterDelay.at(cIndex) -= minTau;
	}
	std::sort(clusterDelay.begin(), clusterDelay.end()); //(7.5-2)

	/* since the scaled Los delays are not to be used in cluster power generation,
	 * we will generate cluster power first and resume to compute Los cluster delay later.*/

	//Step 6: Generate cluster powers.
	doubleVector_t clusterPower;
	double powerSum = 0;
	for (uint8_t cIndex = 0; cIndex < numOfCluster; cIndex++)
	{
		double power = exp(-1 * clusterDelay.at(cIndex) * (table3gpp->m_rTau - 1) / table3gpp->m_rTau / DS) *
					   pow(10, -1 * m_normalRv->GetValue() * table3gpp->m_shadowingStd / 10); //(7.5-5)
		powerSum += power;
		clusterPower.push_back(power);
	}
	double powerMax = 0;

	for (uint8_t cIndex = 0; cIndex < numOfCluster; cIndex++)
	{
		clusterPower.at(cIndex) = clusterPower.at(cIndex) / powerSum; //(7.5-6)
	}

	doubleVector_t clusterPowerForAngles; // this power is only for equation (7.5-9) and (7.5-14), not for (7.5-22)
	if (los)
	{
		double K_linear = pow(10, K_factor / 10);

		for (uint8_t cIndex = 0; cIndex < numOfCluster; cIndex++)
		{
			if (cIndex == 0)
			{
				clusterPowerForAngles.push_back(clusterPower.at(cIndex) / (1 + K_linear) + K_linear / (1 + K_linear)); //(7.5-8)
			}
			else
			{
				clusterPowerForAngles.push_back(clusterPower.at(cIndex) / (1 + K_linear)); //(7.5-8)
			}
			if (powerMax < clusterPowerForAngles.at(cIndex))
			{
				powerMax = clusterPowerForAngles.at(cIndex);
			}
		}
	}
	else
	{
		for (uint8_t cIndex = 0; cIndex < numOfCluster; cIndex++)
		{
			clusterPowerForAngles.push_back(clusterPower.at(cIndex)); //(7.5-6)
			if (powerMax < clusterPowerForAngles.at(cIndex))
			{
				powerMax = clusterPowerForAngles.at(cIndex);
			}
		}
	}

	//remove clusters with less than -25 dB power compared to the maxim cluster power;
	//double thresh = pow(10,-2.5);
	double thresh = 0.0032;
	for (uint8_t cIndex = numOfCluster; cIndex > 0; cIndex--)
	{
		if (clusterPowerForAngles.at(cIndex - 1) < thresh * powerMax)
		{
			clusterPowerForAngles.erase(clusterPowerForAngles.begin() + cIndex - 1);
			clusterPower.erase(clusterPower.begin() + cIndex - 1);
			clusterDelay.erase(clusterDelay.begin() + cIndex - 1);
		}
	}
	uint8_t numReducedCluster = clusterPower.size();

	channelParams->m_numCluster = numReducedCluster;
	// Resume step 5 to compute the delay for LoS condition.
	if (los)
	{
		double C_tau = 0.7705 - 0.0433 * K_factor + 2e-4 * pow(K_factor, 2) + 17e-6 * pow(K_factor, 3); //(7.5-3)
		for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
		{
			clusterDelay.at(cIndex) = clusterDelay.at(cIndex) / C_tau; //(7.5-4)
		}
	}

	/*for (uint8_t i = 0; i < clusterPowerForAngles.size(); i++)
	{
		std::cout <<clusterPowerForAngles.at(i)<<"s\t";
	}
	std::cout << "\n";*/

	/*std::cout << "Delay:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterDelay.at(i)<<"s\t";
	}
	std::cout << "\n";
	std::cout << "Power:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterPower.at(i)<<"\t";
	}
	std::cout << "\n";*/

	//step 7: Generate arrival and departure angles for both azimuth and elevation.

	double C_NLOS, C_phi;
	//According to table 7.5-6, only cluster number equals to 8, 10, 11, 12, 19 and 20 is valid.
	//Not sure why the other cases are in Table 7.5-2.
	switch (numOfCluster) // Table 7.5-2
	{
	case 4:
		C_NLOS = 0.779;
		break;
	case 5:
		C_NLOS = 0.860;
		break;
	case 8:
		C_NLOS = 1.018;
		break;
	case 10:
		C_NLOS = 1.090;
		break;
	case 11:
		C_NLOS = 1.123;
		break;
	case 12:
		C_NLOS = 1.146;
		break;
	case 14:
		C_NLOS = 1.190;
		break;
	case 15:
		C_NLOS = 1.221;
		break;
	case 16:
		C_NLOS = 1.226;
		break;
	case 19:
		C_NLOS = 1.273;
		break;
	case 20:
		C_NLOS = 1.289;
		break;
	default:
		NS_FATAL_ERROR("Invalide cluster number");
	}

	if (los)
	{
		C_phi = C_NLOS * (1.1035 - 0.028 * K_factor - 2e-3 * pow(K_factor, 2) + 1e-4 * pow(K_factor, 3)); //(7.5-10))
	}
	else
	{
		C_phi = C_NLOS; //(7.5-10))
	}

	double C_theta;
	switch (numOfCluster) //Table 7.5-4
	{
	case 8:
		C_NLOS = 0.889;
		break;
	case 10:
		C_NLOS = 0.957;
		break;
	case 11:
		C_NLOS = 1.031;
		break;
	case 12:
		C_NLOS = 1.104;
		break;
	case 19:
		C_NLOS = 1.184;
		break;
	case 20:
		C_NLOS = 1.178;
		break;
	default:
		NS_FATAL_ERROR("Invalide cluster number");
	}

	if (los)
	{
		C_theta = C_NLOS * (1.3086 + 0.0339 * K_factor - 0.0077 * pow(K_factor, 2) + 2e-4 * pow(K_factor, 3)); //(7.5-15)
	}
	else
	{
		C_theta = C_NLOS;
	}

	doubleVector_t clusterAoa, clusterAod, clusterZoa, clusterZod;
	double angle;
	for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
	{
		angle = 2 * ASA * sqrt(-1 * log(clusterPowerForAngles.at(cIndex) / powerMax)) / 1.4 / C_phi; //(7.5-9)
		clusterAoa.push_back(angle);
		angle = 2 * ASD * sqrt(-1 * log(clusterPowerForAngles.at(cIndex) / powerMax)) / 1.4 / C_phi; //(7.5-9)
		clusterAod.push_back(angle);
		angle = -1 * ZSA * log(clusterPowerForAngles.at(cIndex) / powerMax) / C_theta; //(7.5-14)
		clusterZoa.push_back(angle);
		angle = -1 * ZSD * log(clusterPowerForAngles.at(cIndex) / powerMax) / C_theta;
		clusterZod.push_back(angle);
	}

	for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
	{
		int Xn = 1;
		if (m_uniformRv->GetValue(0, 1) < 0.5)
		{
			Xn = -1;
		}
		clusterAoa.at(cIndex) = clusterAoa.at(cIndex) * Xn + (m_normalRv->GetValue() * ASA / 7) + rxAngle.phi * 180 / M_PI; //(7.5-11)
		clusterAod.at(cIndex) = clusterAod.at(cIndex) * Xn + (m_normalRv->GetValue() * ASD / 7) + txAngle.phi * 180 / M_PI;
		if (o2i)
		{
			clusterZoa.at(cIndex) = clusterZoa.at(cIndex) * Xn + (m_normalRv->GetValue() * ZSA / 7) + 90; //(7.5-16)
		}
		else
		{
			clusterZoa.at(cIndex) = clusterZoa.at(cIndex) * Xn + (m_normalRv->GetValue() * ZSA / 7) + rxAngle.theta * 180 / M_PI; //(7.5-16)
		}
		clusterZod.at(cIndex) = clusterZod.at(cIndex) * Xn + (m_normalRv->GetValue() * ZSD / 7) + txAngle.theta * 180 / M_PI + table3gpp->m_offsetZOD; //(7.5-19)
	}

	if (los)
	{
		//The 7.5-12 can be rewrite as Theta_n,ZOA = Theta_n,ZOA - (Theta_1,ZOA - Theta_LOS,ZOA) = Theta_n,ZOA - diffZOA,
		//Similar as AOD, ZSA and ZSD.
		double diffAoa = clusterAoa.at(0) - rxAngle.phi * 180 / M_PI;
		double diffAod = clusterAod.at(0) - txAngle.phi * 180 / M_PI;
		double diffZsa = clusterZoa.at(0) - rxAngle.theta * 180 / M_PI;
		double diffZsd = clusterZod.at(0) - txAngle.theta * 180 / M_PI;

		for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
		{
			clusterAoa.at(cIndex) -= diffAoa; //(7.5-12)
			clusterAod.at(cIndex) -= diffAod;
			clusterZoa.at(cIndex) -= diffZsa; //(7.5-17)
			clusterZod.at(cIndex) -= diffZsd;
		}
	}

	double rayAoa_radian[numReducedCluster][raysPerCluster]; //rayAoa_radian[n][m], where n is cluster index, m is ray index
	double rayAod_radian[numReducedCluster][raysPerCluster]; //rayAod_radian[n][m], where n is cluster index, m is ray index
	double rayZoa_radian[numReducedCluster][raysPerCluster]; //rayZoa_radian[n][m], where n is cluster index, m is ray index
	double rayZod_radian[numReducedCluster][raysPerCluster]; //rayZod_radian[n][m], where n is cluster index, m is ray index

	for (uint8_t nInd = 0; nInd < numReducedCluster; nInd++)
	{
		for (uint8_t mInd = 0; mInd < raysPerCluster; mInd++)
		{
			double tempAoa = clusterAoa.at(nInd) + table3gpp->m_cASA * offSetAlpha[mInd]; //(7.5-13)
			while (tempAoa > 360)
			{
				tempAoa -= 360;
			}

			while (tempAoa < 0)
			{
				tempAoa += 360;
			}
			NS_ASSERT_MSG(tempAoa >= 0 && tempAoa <= 360, "the AOA should be the range of [0,360]");
			rayAoa_radian[nInd][mInd] = tempAoa * M_PI / 180;

			double tempAod = clusterAod.at(nInd) + table3gpp->m_cASD * offSetAlpha[mInd];
			while (tempAod > 360)
			{
				tempAod -= 360;
			}

			while (tempAod < 0)
			{
				tempAod += 360;
			}
			NS_ASSERT_MSG(tempAod >= 0 && tempAod <= 360, "the AOD should be the range of [0,360]");
			rayAod_radian[nInd][mInd] = tempAod * M_PI / 180;

			double tempZoa = clusterZoa.at(nInd) + table3gpp->m_cZSA * offSetAlpha[mInd]; //(7.5-18)

			while (tempZoa > 360)
			{
				tempZoa -= 360;
			}

			while (tempZoa < 0)
			{
				tempZoa += 360;
			}

			if (tempZoa > 180)
			{
				tempZoa = 360 - tempZoa;
			}

			NS_ASSERT_MSG(tempZoa >= 0 && tempZoa <= 180, "the ZOA should be the range of [0,180]");
			rayZoa_radian[nInd][mInd] = tempZoa * M_PI / 180;

			double tempZod = clusterZod.at(nInd) + 0.375 * pow(10, table3gpp->m_uLgZSD) * offSetAlpha[mInd]; //(7.5-20)

			while (tempZod > 360)
			{
				tempZod -= 360;
			}

			while (tempZod < 0)
			{
				tempZod += 360;
			}
			if (tempZod > 180)
			{
				tempZod = 360 - tempZod;
			}
			NS_ASSERT_MSG(tempZod >= 0 && tempZod <= 180, "the ZOD should be the range of [0,180]");
			rayZod_radian[nInd][mInd] = tempZod * M_PI / 180;
		}
	}
	doubleVector_t angle_degree;
	double sizeTemp = clusterZoa.size();
	for (uint8_t ind = 0; ind < 4; ind++)
	{
		switch (ind)
		{
		case 0:
			angle_degree = clusterAoa;
			break;
		case 1:
			angle_degree = clusterZoa;
			break;
		case 2:
			angle_degree = clusterAod;
			break;
		case 3:
			angle_degree = clusterZod;
			break;
		default:
			NS_FATAL_ERROR("Programming Error");
		}

		for (uint8_t nIndex = 0; nIndex < sizeTemp; nIndex++)
		{
			while (angle_degree[nIndex] > 360)
			{
				angle_degree[nIndex] -= 360;
			}

			while (angle_degree[nIndex] < 0)
			{
				angle_degree[nIndex] += 360;
			}

			if (ind == 1 || ind == 3)
			{
				if (angle_degree[nIndex] > 180)
				{
					angle_degree[nIndex] = 360 - angle_degree[nIndex];
				}
			}
		}
		switch (ind)
		{
		case 0:
			clusterAoa = angle_degree;
			break;
		case 1:
			clusterZoa = angle_degree;
			break;
		case 2:
			clusterAod = angle_degree;
			break;
		case 3:
			clusterZod = angle_degree;
			break;
		default:
			NS_FATAL_ERROR("Programming Error");
		}
	}

	doubleVector_t attenuation_dB;
	if (m_blockage)
	{
		attenuation_dB = CalAttenuationOfBlockage(channelParams, clusterAoa, clusterZoa);
		for (uint8_t cInd = 0; cInd < numReducedCluster; cInd++)
		{
			clusterPower.at(cInd) = clusterPower.at(cInd) / pow(10, attenuation_dB.at(cInd) / 10);
		}
	}
	else
	{
		attenuation_dB.push_back(0);
	}

	/*std::cout << "BlockedPower:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterPower.at(i)<<"\t";
	}
	std::cout << "\n";

	std::cout << "AOD:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterAod.at(i)<<"'\t";
	}
	std::cout << "\n";

	std::cout << "AOA:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterAoa.at(i)<<"'\t";
	}
	std::cout << "\n";

	std::cout << "ZOD:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterZod.at(i)<<"'\t";
		for (uint8_t d = 0; d < raysPerCluster; d++)
		{
			std::cout <<rayZod_radian[i][d]<<"\t";
		}
		std::cout << "\n";
	}
	std::cout << "\n";

	std::cout << "ZOA:";
	for (uint8_t i = 0; i < numReducedCluster; i++)
	{
		std::cout <<clusterZoa.at(i)<<"'\t";
	}
	std::cout << "\n";*/

	//Step 8: Coupling of rays within a cluster for both azimuth and elevation
	//shuffle all the arrays to perform random coupling
	for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
	{
		std::shuffle(&rayAod_radian[cIndex][0], &rayAod_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 100));
		std::shuffle(&rayAoa_radian[cIndex][0], &rayAoa_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 200));
		std::shuffle(&rayZod_radian[cIndex][0], &rayZod_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 300));
		std::shuffle(&rayZoa_radian[cIndex][0], &rayZoa_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 400));
	}

	//Step 9: Generate the cross polarization power ratios
	//This step is located in below procedure

	//Step 10: Draw initial phases
	// We add the two 'clusterPhase' variable for implementing polarization. 2018.07.11 shlim.

	double3DVector_t clusterPhase_3D; // rayAoa_radian[n][m][k], where n is cluster index, m is ray index,
									  // and k=0 -> thetatheta, k=1 -> thetaphi, k=2 -> phitheta, k=3 -> phiphi. 2018.07.10 shlim.
	double2DVector_t clusterPhase_2D; //rayAoa_radian[n][m], where n is cluster index, m is ray index

	if (m_crossPolar)
	{
		for (uint8_t i = 0; i < 4; i++)
		{
			double2DVector_t phaseTemp;
			for (uint8_t nInd = 0; nInd < numReducedCluster; nInd++)
			{
				doubleVector_t temp;
				for (uint8_t mInd = 0; mInd < raysPerCluster; mInd++)
				{
					temp.push_back(m_uniformRv->GetValue(-1 * M_PI, M_PI));
				}
				phaseTemp.push_back(temp);
			}
			clusterPhase_3D.push_back(phaseTemp);
		}
		channelParams->m_clusterPhase_3D = clusterPhase_3D;
	}
	else
	{
		for (uint8_t nInd = 0; nInd < numReducedCluster; nInd++)
		{
			doubleVector_t temp;
			for (uint8_t mInd = 0; mInd < raysPerCluster; mInd++)
			{
				temp.push_back(m_uniformRv->GetValue(-1 * M_PI, M_PI));
			}
			clusterPhase_2D.push_back(temp);
		}
		channelParams->m_clusterPhase_2D = clusterPhase_2D;
	}

	double losPhase = m_uniformRv->GetValue(-1 * M_PI, M_PI);

	channelParams->m_losPhase = losPhase;

	//Step 11: Generate channel coefficients for each cluster n and each receiver and transmitter element pair u,s.

	complex3DVector_t H_NLOS; // channel coefficients H_NLOS [u][s][n],
							  // where u and s are receive and transmit antenna element, n is cluster index.
	//180709-jskim14-consider NR tx mode
	uint16_t uSize;
	uint16_t sSize;
	if (m_nrTxMode == 1)
	{
		uSize = rxAntennaNum[0] * rxAntennaNum[1] * rxAntennaNum[2];
		sSize = txAntennaNum[0] * txAntennaNum[1] * txAntennaNum[2];
	}
	else
	{
		uSize = rxAntennaNum[0] * rxAntennaNum[1];
		sSize = txAntennaNum[0] * txAntennaNum[1];
	}
	//jskim14-end

	uint8_t cluster1st = 0, cluster2nd = 0; // first and second strongest cluster;
	double maxPower = 0;
	for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
	{
		if (maxPower < clusterPower.at(cIndex))
		{
			maxPower = clusterPower.at(cIndex);
			cluster1st = cIndex;
		}
	}
	maxPower = 0;
	for (uint8_t cIndex = 0; cIndex < numReducedCluster; cIndex++)
	{
		if (maxPower < clusterPower.at(cIndex) && cluster1st != cIndex)
		{
			maxPower = clusterPower.at(cIndex);
			cluster2nd = cIndex;
		}
	}

	NS_LOG_INFO("1st strongest cluster:" << (unsigned)cluster1st << ", 2nd strongest cluster:" << (unsigned)cluster2nd);

	complex3DVector_t H_usn; //channel coffecient H_usn[u][s][n];
	//Since each of the strongest 2 clusters are divided into 3 sub-clusters, the total cluster will be numReducedCLuster + 4.

	H_usn.resize(uSize);
	for (uint16_t uIndex = 0; uIndex < uSize; uIndex++)
	{
		H_usn.at(uIndex).resize(sSize);
		for (uint16_t sIndex = 0; sIndex < sSize; sIndex++)
		{
			H_usn.at(uIndex).at(sIndex).resize(numReducedCluster);
		}
	}

	// The following for loops computes the channel coefficients
	// We add the polarization scheme by adding the initialPhase_tt,tp,pt,pp. 2018.07.11 shlim.
	for (uint16_t uIndex = 0; uIndex < uSize; uIndex++)
	{
		//Vector uLoc = rxAntenna->GetAntennaLocation(uIndex, rxAntennaNum);
		Vector relativeSpeed = speed;
		NS_LOG_INFO("Relative speed=" << relativeSpeed.x << ", " << relativeSpeed.y << ", " << relativeSpeed.z);
		Angles velocity_angle = Angles(relativeSpeed);
		NS_LOG_INFO("Velocity angle: theta=" << velocity_angle.theta * 180 / M_PI << ", phi=" << velocity_angle.phi * 180 / M_PI);

		Ptr<MobilityModel> txMob, rxMob;
		Vector txLoc, rxLoc;
		if (m_nrTxMode == 1) //180724-jskim14 fix bug
		{
			txMob = txAntenna->GetNetDevice()->GetNode()->GetObject<MobilityModel>();
			rxMob = rxAntenna->GetNetDevice()->GetNode()->GetObject<MobilityModel>();
			txLoc = txMob->GetPosition();
			rxLoc = rxMob->GetPosition();
		}

		//180709-jskim14-consider NR tx mode
		Vector uLoc;
		if (m_nrTxMode == 1)
		{
			uLoc = rxAntenna->GetAntennaLocation(uIndex, rxAntennaNum[0], rxAntennaNum[1], rxAntennaNum[2]);
		}
		else
		{
			uLoc = rxAntenna->GetAntennaLocation(uIndex, rxAntennaNum);
		}
		//jskim14-end

		for (uint16_t sIndex = 0; sIndex < sSize; sIndex++)
		{
			//180709-jskim14-consider NR tx mode
			Vector sLoc;
			if (m_nrTxMode == 1)
			{
				sLoc = txAntenna->GetAntennaLocation(sIndex, txAntennaNum[0], txAntennaNum[1], txAntennaNum[2]);
			}
			else
			{
				sLoc = txAntenna->GetAntennaLocation(sIndex, txAntennaNum);
			}
			//jskim14-end

			if (m_crossPolar) //180724-jskim14-fix bug, change the location of condition statement
			{
				for (uint8_t nIndex = 0; nIndex < numReducedCluster; nIndex++)
				{
					//Compute the N-2 weakest cluster, with cross-polarization. (7.5-22) - 2018.07.03 shlim.
					//double Xnm = m_normalRv->GetValue();
					//double Knm = pow(10, ((table3gpp->m_sigXpr * Xnm + table3gpp->m_uXpr)) / 10);
					if (nIndex != cluster1st && nIndex != cluster2nd)
					{
						std::complex<double> rays(0, 0);
						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{
							double Xnm = m_normalRv->GetValue();
							double Knm = pow(10, ((table3gpp->m_sigXpr * Xnm + table3gpp->m_uXpr)) / 10);

							double initialPhase_tt = clusterPhase_3D.at(0).at(nIndex).at(mIndex);
							double initialPhase_tp = clusterPhase_3D.at(1).at(nIndex).at(mIndex);
							double initialPhase_pt = clusterPhase_3D.at(2).at(nIndex).at(mIndex);
							double initialPhase_pp = clusterPhase_3D.at(3).at(nIndex).at(mIndex);

							Vector2D rxRadiationField = rxAntenna->GetRadiationPattern_polar(rayZoa_radian[nIndex][mIndex], rayAoa_radian[nIndex][mIndex], uIndex);
							Vector2D txRadiationField = txAntenna->GetRadiationPattern_polar(rayZod_radian[nIndex][mIndex], rayAod_radian[nIndex][mIndex], sIndex);

							NS_LOG_INFO("NLOS Radiation field: rx theta=" << rxRadiationField.x << ", rx phi=" << rxRadiationField.y << ", tx theta=" << txRadiationField.x << ", tx phi=" << txRadiationField.y);
							NS_LOG_INFO("Initial phase_tt=" << initialPhase_tt << ", Initial phase_tp=" << initialPhase_tp << ", Initial phase_pt=" << initialPhase_pt << ", Initial phase_pp=" << initialPhase_pp);

							std::complex<double> initialPhase(0, 0);
							initialPhase = (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tt)) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pt)) / sqrt(Knm)) * txRadiationField.x +
										   (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tp)) / sqrt(Knm) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pp))) * txRadiationField.y;
							NS_LOG_INFO("Rx radiation pattern_t=" << rxRadiationField.x << "Rx radiation pattern_p=" << rxRadiationField.y << "Tx radiation pattern_t=" << txRadiationField.x << "Tx radiation pattern_p=" << txRadiationField.y);
							NS_LOG_INFO("Initial phase=" << initialPhase);
							//lambda_0 is accounted in the antenna spacing uLoc and sLoc.
							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * (uLoc.x + rxLoc.x * m_frequency / 3e8) + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * (uLoc.y + rxLoc.y * m_frequency / 3e8) + cos(rayZoa_radian[nIndex][mIndex]) * (uLoc.z + rxLoc.z * m_frequency / 3e8));
							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * (sLoc.x + txLoc.x * m_frequency / 3e8) + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * (sLoc.y + txLoc.y * m_frequency / 3e8) + cos(rayZod_radian[nIndex][mIndex]) * (sLoc.z + txLoc.z * m_frequency / 3e8));
							//Doppler is computed in the CalBeamformingGain function and is simplified to only account for the center anngle of each cluster.
							// double doppler;
							// if (relativeSpeed.GetLength()==0) //this means relative speed is 0
							// {
							// 	doppler = 0;
							// }
							// else
							// {
							// 	doppler = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) +
							// 						  sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) +
							// 						  cos(rayZoa_radian[nIndex][mIndex]) * relativeSpeed.z * cos(velocity_angle.theta)) * slotTime * m_frequency / 3e8;
							// }
							// NS_LOG_INFO("Initial phase=" << initialPhase << ", Rx phase diff=" << rxPhaseDiff << ", Tx phase diff=" << txPhaseDiff << ", Doppler=" << doppler);
							rays += initialPhase *
									exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
							// * exp(std::complex<double>(0, doppler));
							//rays += 1;
						}
						//rays *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						rays *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = rays;
						//NS_LOG_UNCOND("Channel for N-2 weakest clusters=" << rays);
					}
					else //(7.5-28)
					{
						double Xnm = m_normalRv->GetValue();
						double Knm = pow(10, ((table3gpp->m_sigXpr * Xnm + table3gpp->m_uXpr)) / 10);

						std::complex<double> raysSub1(0, 0);
						std::complex<double> raysSub2(0, 0);
						std::complex<double> raysSub3(0, 0);

						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{

							//ZML:Just remind me that the angle offsets for the 3 subclusters were not generated correctly.
							double initialPhase_tt = clusterPhase_3D.at(0).at(nIndex).at(mIndex);
							double initialPhase_tp = clusterPhase_3D.at(1).at(nIndex).at(mIndex);
							double initialPhase_pt = clusterPhase_3D.at(2).at(nIndex).at(mIndex);
							double initialPhase_pp = clusterPhase_3D.at(3).at(nIndex).at(mIndex);

							Vector2D rxRadiationField = rxAntenna->GetRadiationPattern_polar(rayZoa_radian[nIndex][mIndex], rayAoa_radian[nIndex][mIndex], uIndex);
							Vector2D txRadiationField = txAntenna->GetRadiationPattern_polar(rayZod_radian[nIndex][mIndex], rayAod_radian[nIndex][mIndex], sIndex);

							NS_LOG_INFO("NLOS Radiation field: rx theta=" << rxRadiationField.x << ", rx phi=" << rxRadiationField.y << ", tx theta=" << txRadiationField.x << ", tx phi=" << txRadiationField.y);

							std::complex<double> initialPhase(0, 0);
							initialPhase = (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tt)) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pt)) / sqrt(Knm)) * txRadiationField.x +
										   (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tp)) / sqrt(Knm) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pp))) * txRadiationField.y;

							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * (uLoc.x + rxLoc.x * m_frequency / 3e8) + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * (uLoc.y + rxLoc.y * m_frequency / 3e8) + cos(rayZoa_radian[nIndex][mIndex]) * (uLoc.z + rxLoc.z * m_frequency / 3e8));
							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * (sLoc.x + txLoc.x * m_frequency / 3e8) + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * (sLoc.y + txLoc.y * m_frequency / 3e8) + cos(rayZod_radian[nIndex][mIndex]) * (sLoc.z + txLoc.z * m_frequency / 3e8));
							//Doppler is computed in the CalBeamformingGain function and is simplified to only account for the center anngle of each cluster.
							// double doppler;
							// if(relativeSpeed.GetLength()==0)
							// {
							// 	doppler = 0;
							// }
							// else
							// {
							// 	doppler = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) +
							// 						  sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) +
							// 						  cos(rayZoa_radian[nIndex][mIndex]) * relativeSpeed.z * cos(velocity_angle.theta)) * slotTime * m_frequency / 3e8;
							// }
							//double delaySpread;

							switch (mIndex)
							{
							case 9:
							case 10:
							case 11:
							case 12:
							case 17:
							case 18:
								//delaySpread= -2*M_PI*(clusterDelay.at(nIndex)+1.28*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub2 += initialPhase *
											exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								// * exp(std::complex<double>(0, doppler));
								//raysSub2 += 1;
								break;
							case 13:
							case 14:
							case 15:
							case 16:
								//delaySpread = -2*M_PI*(clusterDelay.at(nIndex)+2.56*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub3 += initialPhase *
											exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								// * exp(std::complex<double>(0, doppler));
								//raysSub3 += 1;
								break;
							default: //case 1,2,3,4,5,6,7,8,19,20
								//delaySpread = -2*M_PI*clusterDelay.at(nIndex)*m_phyMacConfig->GetCenterFrequency ();
								raysSub1 += initialPhase *
											exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								// * exp(std::complex<double>(0, doppler));
								//raysSub1 += 1;
								break;
							}
						}
						raysSub1 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub2 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub3 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = raysSub1;
						H_usn.at(uIndex).at(sIndex).push_back(raysSub2);
						H_usn.at(uIndex).at(sIndex).push_back(raysSub3);
					}
				}
				if (los) //(7.5-29) && (7.5-30)
				{
					Vector2D rxRadiationField = rxAntenna->GetRadiationPattern_polar(rxAngle.theta, rxAngle.phi, uIndex);
					Vector2D txRadiationField = txAntenna->GetRadiationPattern_polar(txAngle.theta, txAngle.phi, sIndex);

					std::complex<double> initialPhase(0, 0);
					initialPhase = rxRadiationField.x * txRadiationField.x - rxRadiationField.y * txRadiationField.y;
					NS_LOG_INFO("LOS Radiation field: rx theta=" << rxRadiationField.x << ", rx phi=" << rxRadiationField.y << ", tx theta=" << txRadiationField.x << ", tx phi=" << txRadiationField.y);

					std::complex<double> ray(0, 0);
					double rxPhaseDiff = 2 * M_PI * (sin(rxAngle.theta) * cos(rxAngle.phi) * (uLoc.x + rxLoc.x * m_frequency / 3e8) + sin(rxAngle.theta) * sin(rxAngle.phi) * (uLoc.y + rxLoc.y * m_frequency / 3e8) + cos(rxAngle.theta) * (uLoc.z + rxLoc.z * m_frequency / 3e8));
					double txPhaseDiff = 2 * M_PI * (sin(txAngle.theta) * cos(txAngle.phi) * (sLoc.x + txLoc.x * m_frequency / 3e8) + sin(txAngle.theta) * sin(txAngle.phi) * (sLoc.y + txLoc.y * m_frequency / 3e8) + cos(txAngle.theta) * (sLoc.z + txLoc.z * m_frequency / 3e8));

					//Doppler is computed in the CalBeamformingGain function and is simplified to only account for the center anngle of each cluster.
					// double doppler;
					// if(relativeSpeed.GetLength()==0)
					// {
					// 	doppler = 0;
					// }
					// else
					// {
					// 	doppler	= 2*M_PI*(sin(rxAngle.theta) * cos(rxAngle.phi) * relativeSpeed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) +
					// 					  sin(rxAngle.theta) * sin(rxAngle.phi) * relativeSpeed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) +
					// 					  cos(rxAngle.theta) * relativeSpeed.z * cos(velocity_angle.theta)) * slotTime * m_frequency / 3e8;
					// }
					ray = //exp(std::complex<double>(0, losPhase)) doesnt use losphase in 38.901 (7.5-29)
						initialPhase * exp(std::complex<double>(0, -2 * M_PI * dis3D * m_frequency / 3e8)) *
						exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
					// * exp(std::complex<double>(0, doppler));

					double K_linear = pow(10, K_factor / 10);
					NS_LOG_INFO("LOS case: K-factor linear=" << K_linear << ", initial phase=" << initialPhase);
					// the LOS path should be attenuated if blockage is enabled.
					H_usn.at(uIndex).at(sIndex).at(0) = sqrt(1 / (K_linear + 1)) * H_usn.at(uIndex).at(sIndex).at(0) + sqrt(K_linear / (1 + K_linear)) * ray; // / pow(10, attenuation_dB.at(0) / 10); //(7.5-30) for tau = tau1
					double tempSize = H_usn.at(uIndex).at(sIndex).size();
					for (uint8_t nIndex = 1; nIndex < tempSize; nIndex++)
					{
						H_usn.at(uIndex).at(sIndex).at(nIndex) *= sqrt(1 / (K_linear + 1)); //(7.5-30) for tau = tau2...taunN
					}
					//for (uint8_t nIndex = 1; nIndex < tempSize; nIndex++)
					//{
					//	NS_LOG_INFO("Channel coefficient [" << (unsigned)uIndex << "][" << (unsigned)sIndex << "][" << (unsigned)nIndex << "]=" << H_usn.at(uIndex).at(sIndex).at(nIndex));
					//}
				}
			}
			else
			{
				for (uint8_t nIndex = 0; nIndex < numReducedCluster; nIndex++)
				{
					//Compute the N-2 weakest cluster, only vertical polarization. (7.5-22)
					if (nIndex != cluster1st && nIndex != cluster2nd)
					{
						std::complex<double> rays(0, 0);
						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{

							double initialPhase = clusterPhase_2D.at(nIndex).at(mIndex);
							//lambda_0 is accounted in the antenna spacing uLoc and sLoc.
							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * uLoc.x + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * uLoc.y + cos(rayZoa_radian[nIndex][mIndex]) * uLoc.z);

							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * sLoc.x + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * sLoc.y + cos(rayZod_radian[nIndex][mIndex]) * sLoc.z);
							//Doppler is computed in the CalBeamformingGain function and is simplified to only account for the center anngle of each cluster.
							//double doppler = 2*M_PI*(sin(rayZoa_radian[nIndex][mIndex])*cos(rayAoa_radian[nIndex][mIndex])*relativeSpeed.x
							//		+ sin(rayZoa_radian[nIndex][mIndex])*sin(rayAoa_radian[nIndex][mIndex])*relativeSpeed.y
							//		+ cos(rayZoa_radian[nIndex][mIndex])*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;
							rays += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
							//*exp(std::complex<double>(0, doppler));
							//rays += 1;
						}
						//rays *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						rays *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = rays;
					}

					else //(7.5-28) //nonpolar case. 2018.07.11 shlim.
					{
						std::complex<double> raysSub1(0, 0);
						std::complex<double> raysSub2(0, 0);
						std::complex<double> raysSub3(0, 0);

						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{

							//ZML:Just remind me that the angle offsets for the 3 subclusters were not generated correctly.

							double initialPhase = clusterPhase_2D.at(nIndex).at(mIndex);
							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * uLoc.x + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * uLoc.y + cos(rayZoa_radian[nIndex][mIndex]) * uLoc.z);
							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * sLoc.x + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * sLoc.y + cos(rayZod_radian[nIndex][mIndex]) * sLoc.z);
							//double doppler = 2*M_PI*(sin(rayZoa_radian[nIndex][mIndex])*cos(rayAoa_radian[nIndex][mIndex])*relativeSpeed.x
							//		+ sin(rayZoa_radian[nIndex][mIndex])*sin(rayAoa_radian[nIndex][mIndex])*relativeSpeed.y
							//		+ cos(rayZoa_radian[nIndex][mIndex])*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;
							//double delaySpread;
							switch (mIndex)
							{
							case 9:
							case 10:
							case 11:
							case 12:
							case 17:
							case 18:
								//delaySpread= -2*M_PI*(clusterDelay.at(nIndex)+1.28*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub2 += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//*exp(std::complex<double>(0, doppler));
								//raysSub2 +=1;
								break;
							case 13:
							case 14:
							case 15:
							case 16:
								//delaySpread = -2*M_PI*(clusterDelay.at(nIndex)+2.56*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub3 += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//*exp(std::complex<double>(0, doppler));
								//raysSub3 +=1;
								break;
							default: //case 1,2,3,4,5,6,7,8,19,20
								//delaySpread = -2*M_PI*clusterDelay.at(nIndex)*m_phyMacConfig->GetCenterFrequency ();
								raysSub1 += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//*exp(std::complex<double>(0, doppler));
								//raysSub1 +=1;
								break;
							}
						}
						//raysSub1 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						//raysSub2 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						//raysSub3 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						raysSub1 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub2 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub3 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = raysSub1;
						H_usn.at(uIndex).at(sIndex).push_back(raysSub2);
						H_usn.at(uIndex).at(sIndex).push_back(raysSub3);
					}
				}
				if (los) //(7.5-29) && (7.5-30)
				{
					std::complex<double> ray(0, 0);
					double rxPhaseDiff = 2 * M_PI * (sin(rxAngle.theta) * cos(rxAngle.phi) * uLoc.x + sin(rxAngle.theta) * sin(rxAngle.phi) * uLoc.y + cos(rxAngle.theta) * uLoc.z);
					double txPhaseDiff = 2 * M_PI * (sin(txAngle.theta) * cos(txAngle.phi) * sLoc.x + sin(txAngle.theta) * sin(txAngle.phi) * sLoc.y + cos(txAngle.theta) * sLoc.z);
					//double doppler = 2*M_PI*(sin(rxAngle.theta)*cos(rxAngle.phi)*relativeSpeed.x
					//		+ sin(rxAngle.theta)*sin(rxAngle.phi)*relativeSpeed.y
					//		+ cos(rxAngle.theta)*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;

					ray = exp(std::complex<double>(0, losPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rxAngle.theta) * txAntenna->GetRadiationPattern_nonpolar(txAngle.theta)) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
					//*exp(std::complex<double>(0, doppler));

					double K_linear = pow(10, K_factor / 10);
					// the LOS path should be attenuated if blockage is enabled.
					H_usn.at(uIndex).at(sIndex).at(0) = sqrt(1 / (K_linear + 1)) * H_usn.at(uIndex).at(sIndex).at(0) + sqrt(K_linear / (1 + K_linear)) * ray / pow(10, attenuation_dB.at(0) / 10); //(7.5-30) for tau = tau1
					double tempSize = H_usn.at(uIndex).at(sIndex).size();
					for (uint8_t nIndex = 1; nIndex < tempSize; nIndex++)
					{
						H_usn.at(uIndex).at(sIndex).at(nIndex) *= sqrt(1 / (K_linear + 1)); //(7.5-30) for tau = tau2...taunN
					}
					for (uint8_t nIndex = 1; nIndex < tempSize; nIndex++)
					{
						NS_LOG_INFO("Channel coefficient [" << (unsigned)uIndex << "][" << (unsigned)sIndex << "][" << (unsigned)nIndex << "]=" << H_usn.at(uIndex).at(sIndex).at(nIndex));
					}
				}
			}
		}
	}

	if (cluster1st == cluster2nd)
	{
		clusterDelay.push_back(clusterDelay.at(cluster1st) + 1.28 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(cluster1st) + 2.56 * table3gpp->m_cDS);

		clusterAoa.push_back(clusterAoa.at(cluster1st));
		clusterAoa.push_back(clusterAoa.at(cluster1st));

		clusterZoa.push_back(clusterZoa.at(cluster1st));
		clusterZoa.push_back(clusterZoa.at(cluster1st));

		clusterAod.push_back(clusterAod.at(cluster1st));
		clusterAod.push_back(clusterAod.at(cluster1st));

		clusterZod.push_back(clusterZod.at(cluster1st));
		clusterZod.push_back(clusterZod.at(cluster1st));
	}
	else
	{
		double min, max;
		if (cluster1st < cluster2nd)
		{
			min = cluster1st;
			max = cluster2nd;
		}
		else
		{
			min = cluster2nd;
			max = cluster1st;
		}
		clusterDelay.push_back(clusterDelay.at(min) + 1.28 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(min) + 2.56 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(max) + 1.28 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(max) + 2.56 * table3gpp->m_cDS);

		clusterAoa.push_back(clusterAoa.at(min));
		clusterAoa.push_back(clusterAoa.at(min));
		clusterAoa.push_back(clusterAoa.at(max));
		clusterAoa.push_back(clusterAoa.at(max));

		clusterZoa.push_back(clusterZoa.at(min));
		clusterZoa.push_back(clusterZoa.at(min));
		clusterZoa.push_back(clusterZoa.at(max));
		clusterZoa.push_back(clusterZoa.at(max));

		clusterAod.push_back(clusterAod.at(min));
		clusterAod.push_back(clusterAod.at(min));
		clusterAod.push_back(clusterAod.at(max));
		clusterAod.push_back(clusterAod.at(max));

		clusterZod.push_back(clusterZod.at(min));
		clusterZod.push_back(clusterZod.at(min));
		clusterZod.push_back(clusterZod.at(max));
		clusterZod.push_back(clusterZod.at(max));
	}

	NS_LOG_INFO("size of coefficient matrix =[" << H_usn.size() << "][" << H_usn.at(0).size() << "][" << H_usn.at(0).at(0).size() << "]");

	/*std::cout << "Delay:";
	for (uint8_t i = 0; i < clusterDelay.size(); i++)
	{
		std::cout <<clusterDelay.at(i)<<"s\t";
	}
	std::cout << "\n";*/

	channelParams->m_channel = H_usn;
	channelParams->m_delay = clusterDelay;

	channelParams->m_angle.clear();
	channelParams->m_angle.push_back(clusterAoa);
	channelParams->m_angle.push_back(clusterZoa);
	channelParams->m_angle.push_back(clusterAod);
	channelParams->m_angle.push_back(clusterZod);

	return channelParams;
} // namespace ns3

Ptr<Params3gpp>
MmWave3gppChannel::UpdateChannel(Ptr<Params3gpp> params3gpp, Ptr<ParamsTable> table3gpp,
								 Ptr<AntennaArrayModel> txAntenna, Ptr<AntennaArrayModel> rxAntenna,
								 uint8_t *txAntennaNum, uint8_t *rxAntennaNum, Angles &rxAngle, Angles &txAngle) const
{
	// We also apply the polarization scheme same with the GetNewChannel. 2018.07.11 shlim.
	Ptr<Params3gpp> params = params3gpp;
	uint8_t raysPerCluster = table3gpp->m_raysPerCluster;
	//We first update the current location, the previous location will be updated in the end.
	//double slotTime = Simulator::Now().GetSeconds();
	//Step 4: Get LSP from previous channel
	double DS = params->m_DS;
	double K_factor = params->m_K;

	//Step 5: Update Delays.
	//copy delay from previous channel.
	doubleVector_t clusterDelay;
	for (uint8_t cInd = 0; cInd < params->m_numCluster; cInd++)
	{
		clusterDelay.push_back(params->m_delay.at(cInd));
	}
	//If LOS condition, we need to revert the tau^LOS_n back to tau_n.
	if (params->m_los)
	{
		double C_tau = 0.7705 - 0.0433 * K_factor + 2e-4 * pow(K_factor, 2) + 17e-6 * pow(K_factor, 3); //(7.5-3)
		for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
		{
			clusterDelay.at(cIndex) = clusterDelay.at(cIndex) * C_tau;
		}
	}
	//update delay based on equation (7.6-9)
	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		clusterDelay.at(cIndex) -= (sin(params->m_angle.at(ZOA_INDEX).at(cIndex) * M_PI / 180) * cos(params->m_angle.at(AOA_INDEX).at(cIndex) * M_PI / 180) * params->m_speed.x + sin(params->m_angle.at(ZOA_INDEX).at(cIndex) * M_PI / 180) * sin(params->m_angle.at(AOA_INDEX).at(cIndex) * M_PI / 180) * params->m_speed.y) * m_updatePeriod.GetSeconds() / 3e8; //(7.6-9)
	}

	/* since the scaled Los delays are not to be used in cluster power generation,
	 * we will generate cluster power first and resume to compute Los cluster delay later.*/

	//Step 6: Generate cluster powers.
	doubleVector_t clusterPower;
	double powerSum = 0;
	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		double power = exp(-1 * clusterDelay.at(cIndex) * (table3gpp->m_rTau - 1) / table3gpp->m_rTau / DS) *
					   pow(10, -1 * m_normalRv->GetValue() * table3gpp->m_shadowingStd / 10); //(7.5-5)
		powerSum += power;
		clusterPower.push_back(power);
	}

	// we do not need to compute the cluster power of LOS case, since it is used for generating angles.
	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		clusterPower.at(cIndex) = clusterPower.at(cIndex) / powerSum; //(7.5-6)
	}

	// Resume step 5 to compute the delay for LoS condition.
	if (params->m_los)
	{
		double C_tau = 0.7705 - 0.0433 * K_factor + 2e-4 * pow(K_factor, 2) + 17e-6 * pow(K_factor, 3); //(7.5-3)
		for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
		{
			clusterDelay.at(cIndex) = clusterDelay.at(cIndex) / C_tau; //(7.5-4)
		}
	}

	/*std::cout << "Delay:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterDelay.at(i)<<"s\t";
	}
	std::cout << "\n";
	std::cout << "Power:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterPower.at(i)<<"\t";
	}
	std::cout << "\n";*/

	//step 7: Generate arrival and departure angles for both azimuth and elevation.

	//copy the angles from previous channel
	doubleVector_t clusterAoa, clusterZoa, clusterAod, clusterZod;
	/*
	 * copy the angles from previous channel
	 * need to change the angle according to equations (7.6-11) - (7.6-14)*/
	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		clusterAoa.push_back(params->m_angle.at(AOA_INDEX).at(cIndex));
		clusterZoa.push_back(params->m_angle.at(ZOA_INDEX).at(cIndex));
		clusterAod.push_back(params->m_angle.at(AOD_INDEX).at(cIndex));
		clusterZod.push_back(params->m_angle.at(ZOD_INDEX).at(cIndex));
	}
	double v = sqrt(params->m_speed.x * params->m_speed.x + params->m_speed.y * params->m_speed.y);
	if (v > 1e-6) //Update the angles only when the speed is not 0.
	{
		if (params->m_norRvAngles.size() == 0)
		{
			//initial case
			for (uint8_t cInd = 0; cInd < params->m_numCluster; cInd++)
			{
				doubleVector_t temp;
				temp.push_back(0); //initial random angle for AOA
				temp.push_back(0); //initial random angle for ZOA
				temp.push_back(0); //initial random angle for AOD
				temp.push_back(0); //initial random angle for ZOD
				params->m_norRvAngles.push_back(temp);
			}
		}
		for (uint8_t cInd = 0; cInd < params->m_numCluster; cInd++)
		{
			double timeDiff = Now().GetSeconds() - params->m_generatedTime.GetSeconds();
			double ranPhiAOD, ranThetaZOD, ranPhiAOA, ranThetaZOA;
			if (params->m_los && cInd == 0) //These angles equal 0 for LOS path.
			{
				ranPhiAOD = 0;
				ranThetaZOD = 0;
				ranPhiAOA = 0;
				ranThetaZOA = 0;
			}
			else
			{
				double deltaX = sqrt(pow(params->m_preLocUT.x - params->m_locUT.x, 2) + pow(params->m_preLocUT.y - params->m_locUT.y, 2));
				double R_phi = exp(-1 * deltaX / 50);	// 50 m is the correlation distance as specified in TR 38.900 Sec 7.6.3.2
				double R_theta = exp(-1 * deltaX / 100); // 100 m is the correlation distance as specified in TR 38.900 Sec 7.6.3.2

				//In order to generate correlated uniform random variables, we first generate correlated normal random variables and map the normal RV to uniform RV.
				//Notice the correlation will change if the RV is transformed from normal to uniform.
				//To compensate the distortion, the correlation of the normal RV is computed
				//such that the uniform RV would have the desired correlation when transformed from normal RV.

				//The following formula was obtained from MATLAB numerical simulation.

				if (R_phi * R_phi * (-0.069) + R_phi * 1.074 - 0.002 < 1) //When the correlation for normal RV is close to 1, no need to transform.
				{
					R_phi = R_phi * R_phi * (-0.069) + R_phi * 1.074 - 0.002;
				}
				if (R_theta * R_theta * (-0.069) + R_theta * 1.074 - 0.002 < 1)
				{
					R_theta = R_theta * R_theta * (-0.069) + R_theta * 1.074 - 0.002;
				}

				//We can generate a new correlated normal RV with the following formula
				params->m_norRvAngles.at(cInd).at(AOD_INDEX) = R_phi * params->m_norRvAngles.at(cInd).at(AOD_INDEX) + sqrt(1 - R_phi * R_phi) * m_normalRv->GetValue();
				params->m_norRvAngles.at(cInd).at(ZOD_INDEX) = R_theta * params->m_norRvAngles.at(cInd).at(ZOD_INDEX) + sqrt(1 - R_theta * R_theta) * m_normalRv->GetValue();
				params->m_norRvAngles.at(cInd).at(AOA_INDEX) = R_phi * params->m_norRvAngles.at(cInd).at(AOA_INDEX) + sqrt(1 - R_phi * R_phi) * m_normalRv->GetValue();
				params->m_norRvAngles.at(cInd).at(ZOA_INDEX) = R_theta * params->m_norRvAngles.at(cInd).at(ZOA_INDEX) + sqrt(1 - R_theta * R_theta) * m_normalRv->GetValue();

				//The normal RV is transformed to uniform RV with the desired correlation.
				ranPhiAOD = (0.5 * erfc(-1 * params->m_norRvAngles.at(cInd).at(AOD_INDEX) / sqrt(2))) * 2 * M_PI - M_PI;
				ranThetaZOD = (0.5 * erfc(-1 * params->m_norRvAngles.at(cInd).at(ZOD_INDEX) / sqrt(2))) * M_PI - 0.5 * M_PI;
				ranPhiAOA = (0.5 * erfc(-1 * params->m_norRvAngles.at(cInd).at(AOA_INDEX) / sqrt(2))) * 2 * M_PI - M_PI;
				ranThetaZOA = (0.5 * erfc(-1 * params->m_norRvAngles.at(cInd).at(ZOA_INDEX) / sqrt(2))) * M_PI - 0.5 * M_PI;
			}
			clusterAod.at(cInd) += v * timeDiff *
								   sin(atan(params->m_speed.y / params->m_speed.x) - clusterAod.at(cInd) * M_PI / 180 + ranPhiAOD) * 180 / (M_PI * params->m_dis2D);
			clusterZod.at(cInd) -= v * timeDiff *
								   cos(atan(params->m_speed.y / params->m_speed.x) - clusterAod.at(cInd) * M_PI / 180 + ranThetaZOD) * 180 / (M_PI * params->m_dis3D);
			clusterAoa.at(cInd) -= v * timeDiff *
								   sin(atan(params->m_speed.y / params->m_speed.x) - clusterAoa.at(cInd) * M_PI / 180 + ranPhiAOA) * 180 / (M_PI * params->m_dis2D);
			clusterZoa.at(cInd) -= v * timeDiff *
								   cos(atan(params->m_speed.y / params->m_speed.x) - clusterAoa.at(cInd) * M_PI / 180 + ranThetaZOA) * 180 / (M_PI * params->m_dis3D);
		}
	}

	double rayAoa_radian[params->m_numCluster][raysPerCluster]; //rayAoa_radian[n][m], where n is cluster index, m is ray index
	double rayAod_radian[params->m_numCluster][raysPerCluster]; //rayAod_radian[n][m], where n is cluster index, m is ray index
	double rayZoa_radian[params->m_numCluster][raysPerCluster]; //rayZoa_radian[n][m], where n is cluster index, m is ray index
	double rayZod_radian[params->m_numCluster][raysPerCluster]; //rayZod_radian[n][m], where n is cluster index, m is ray index

	for (uint8_t nInd = 0; nInd < params->m_numCluster; nInd++)
	{
		for (uint8_t mInd = 0; mInd < raysPerCluster; mInd++)
		{
			double tempAoa = clusterAoa.at(nInd) + table3gpp->m_cASA * offSetAlpha[mInd]; //(7.5-13)
			while (tempAoa > 360)
			{
				tempAoa -= 360;
			}

			while (tempAoa < 0)
			{
				tempAoa += 360;
			}
			NS_ASSERT_MSG(tempAoa >= 0 && tempAoa <= 360, "the AOA should be the range of [0,360]");
			rayAoa_radian[nInd][mInd] = tempAoa * M_PI / 180;

			double tempAod = clusterAod.at(nInd) + table3gpp->m_cASD * offSetAlpha[mInd];
			while (tempAod > 360)
			{
				tempAod -= 360;
			}

			while (tempAod < 0)
			{
				tempAod += 360;
			}
			NS_ASSERT_MSG(tempAod >= 0 && tempAod <= 360, "the AOD should be the range of [0,360]");
			rayAod_radian[nInd][mInd] = tempAod * M_PI / 180;

			double tempZoa = clusterZoa.at(nInd) + table3gpp->m_cZSA * offSetAlpha[mInd]; //(7.5-18)

			while (tempZoa > 360)
			{
				tempZoa -= 360;
			}

			while (tempZoa < 0)
			{
				tempZoa += 360;
			}

			if (tempZoa > 180)
			{
				tempZoa = 360 - tempZoa;
			}

			NS_ASSERT_MSG(tempZoa >= 0 && tempZoa <= 180, "the ZOA should be the range of [0,180]");
			rayZoa_radian[nInd][mInd] = tempZoa * M_PI / 180;

			double tempZod = clusterZod.at(nInd) + 0.375 * pow(10, table3gpp->m_uLgZSD) * offSetAlpha[mInd]; //(7.5-20)

			while (tempZod > 360)
			{
				tempZod -= 360;
			}

			while (tempZod < 0)
			{
				tempZod += 360;
			}
			if (tempZod > 180)
			{
				tempZod = 360 - tempZod;
			}
			NS_ASSERT_MSG(tempZod >= 0 && tempZod <= 180, "the ZOD should be the range of [0,180]");
			rayZod_radian[nInd][mInd] = tempZod * M_PI / 180;
		}
	}

	doubleVector_t angle_degree;
	double sizeTemp = clusterZoa.size();
	for (uint8_t ind = 0; ind < 4; ind++)
	{
		switch (ind)
		{
		case 0:
			angle_degree = clusterAoa;
			break;
		case 1:
			angle_degree = clusterZoa;
			break;
		case 2:
			angle_degree = clusterAod;
			break;
		case 3:
			angle_degree = clusterZod;
			break;
		default:
			NS_FATAL_ERROR("Programming Error");
		}

		for (uint8_t nIndex = 0; nIndex < sizeTemp; nIndex++)
		{
			while (angle_degree[nIndex] > 360)
			{
				angle_degree[nIndex] -= 360;
			}

			while (angle_degree[nIndex] < 0)
			{
				angle_degree[nIndex] += 360;
			}

			if (ind == 1 || ind == 3)
			{
				if (angle_degree[nIndex] > 180)
				{
					angle_degree[nIndex] = 360 - angle_degree[nIndex];
				}
			}
		}
		switch (ind)
		{
		case 0:
			clusterAoa = angle_degree;
			break;
		case 1:
			clusterZoa = angle_degree;
			break;
		case 2:
			clusterAod = angle_degree;
			break;
		case 3:
			clusterZod = angle_degree;
			break;
		default:
			NS_FATAL_ERROR("Programming Error");
		}
	}
	doubleVector_t attenuation_dB;
	if (m_blockage)
	{
		attenuation_dB = CalAttenuationOfBlockage(params, clusterAoa, clusterZoa);
		for (uint8_t cInd = 0; cInd < params->m_numCluster; cInd++)
		{
			clusterPower.at(cInd) = clusterPower.at(cInd) / pow(10, attenuation_dB.at(cInd) / 10);
		}
	}
	else
	{
		attenuation_dB.push_back(0);
	}

	/*std::cout << "BlockedPower:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterPower.at(i)<<"\t";
	}
	std::cout << "\n";*/

	/*std::cout << "AOD:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterAod.at(i)<<"'\t";
	}
	std::cout << "\n";

	std::cout << "AOA:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterAoa.at(i)<<"'\t";
	}
	std::cout << "\n";

	std::cout << "ZOD:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterZod.at(i)<<"'\t";
		for (uint8_t d = 0; d < raysPerCluster; d++)
		{
			std::cout <<rayZod_radian[i][d]<<"\t";
		}
		std::cout << "\n";
	}
	std::cout << "\n";

	std::cout << "ZOA:";
	for (uint8_t i = 0; i < params->m_numCluster; i++)
	{
		std::cout <<clusterZoa.at(i)<<"'\t";
	}
	std::cout << "\n";*/

	//Step 8: Coupling of rays within a cluster for both azimuth and elevation
	//shuffle all the arrays to perform random coupling

	//since the updating and original generated angles should have the same order of "random coupling",
	//I control the seed of each shuffle, so that the update and original generated angle use the same seed.
	//Is this correct?

	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		std::shuffle(&rayAod_radian[cIndex][0], &rayAod_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 100));
		std::shuffle(&rayAoa_radian[cIndex][0], &rayAoa_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 200));
		std::shuffle(&rayZod_radian[cIndex][0], &rayZod_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 300));
		std::shuffle(&rayZoa_radian[cIndex][0], &rayZoa_radian[cIndex][raysPerCluster], std::default_random_engine(cIndex * 1000 + 400));
	}

	//Step 9: Generate the cross polarization power ratios
	//This step is skipped, only vertical polarization is considered in this version

	double3DVector_t clusterPhase_3D;
	double2DVector_t clusterPhase_2D;
	//Step 10: Draw initial phases
	if (m_crossPolar)
	{
		clusterPhase_3D = params->m_clusterPhase_3D;
	}
	else
	{
		clusterPhase_2D = params->m_clusterPhase_2D; //rayAoa_radian[n][m], where n is cluster index, m is ray index
	}

	double losPhase = params->m_losPhase;
	// these two should also be generated from previous channel.

	//Step 11: Generate channel coefficients for each cluster n and each receiver and transmitter element pair u,s.

	complex3DVector_t H_NLOS; // channel coefficients H_NLOS [u][s][n],
							  // where u and s are receive and transmit antenna element, n is cluster index.
	//180709-jskim14-consider NR tx mode
	uint16_t uSize;
	uint16_t sSize;
	if (m_nrTxMode == 1)
	{
		uSize = rxAntennaNum[0] * rxAntennaNum[1] * rxAntennaNum[2];
		sSize = txAntennaNum[0] * txAntennaNum[1] * txAntennaNum[2];
	}
	else
	{
		uSize = rxAntennaNum[0] * rxAntennaNum[1];
		sSize = txAntennaNum[0] * txAntennaNum[1];
	}

	uint8_t cluster1st = 0, cluster2nd = 0; // first and second strongest cluster;
	double maxPower = 0;
	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		if (maxPower < clusterPower.at(cIndex))
		{
			maxPower = clusterPower.at(cIndex);
			cluster1st = cIndex;
		}
	}
	maxPower = 0;
	for (uint8_t cIndex = 0; cIndex < params->m_numCluster; cIndex++)
	{
		if (maxPower < clusterPower.at(cIndex) && cluster1st != cIndex)
		{
			maxPower = clusterPower.at(cIndex);
			cluster2nd = cIndex;
		}
	}

	NS_LOG_INFO("1st strongest cluster:" << (int)cluster1st << ", 2nd strongest cluster:" << (int)cluster2nd);

	complex3DVector_t H_usn; //channel coffecient H_usn[u][s][n];
	//Since each of the strongest 2 clusters are divided into 3 sub-clusters, the total cluster will be numReducedCLuster + 4.

	H_usn.resize(uSize);
	for (uint16_t uIndex = 0; uIndex < uSize; uIndex++)
	{
		H_usn.at(uIndex).resize(sSize);
		for (uint16_t sIndex = 0; sIndex < sSize; sIndex++)
		{
			H_usn.at(uIndex).at(sIndex).resize(params->m_numCluster);
		}
	}
	//double slotTime = Simulator::Now ().GetSeconds ();
	// The following for loops computes the channel coefficients
	for (uint16_t uIndex = 0; uIndex < uSize; uIndex++)
	{
		//180709-jskim14-consider NR tx mode
		Vector uLoc;
		if (m_nrTxMode == 1)
		{
			uLoc = rxAntenna->GetAntennaLocation(uIndex, rxAntennaNum[0], rxAntennaNum[1], rxAntennaNum[2]);
		}
		else
		{
			uLoc = rxAntenna->GetAntennaLocation(uIndex, rxAntennaNum);
		}
		//jskim14-end

		for (uint16_t sIndex = 0; sIndex < sSize; sIndex++)
		{
			//Vector sLoc = txAntenna->GetAntennaLocation(sIndex, txAntennaNum);
			//Vector relativeSpeed = params->m_speed;
			//Angles velocity_angle = Angles(relativeSpeed);

			//180709-jskim14-consider NR tx mode
			Vector sLoc;
			if (m_nrTxMode == 1)
			{
				sLoc = txAntenna->GetAntennaLocation(sIndex, txAntennaNum[0], txAntennaNum[1], txAntennaNum[2]);
			}
			else
			{
				sLoc = txAntenna->GetAntennaLocation(sIndex, txAntennaNum);
			}
			//jskim14-end

			//Compute the N-2 weakest cluster, with cross-polarization. (7.5-22) - 2018.07.03 shlim.
			if (m_crossPolar)
			{
				double Xnm = m_normalRv->GetValue();
				double Knm = pow(10, ((table3gpp->m_sigXpr * Xnm + table3gpp->m_uXpr)) / 10);

				for (uint8_t nIndex = 0; nIndex < params->m_numCluster; nIndex++)
				{
					if (nIndex != cluster1st && nIndex != cluster2nd)
					{
						std::complex<double> rays(0, 0);
						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{
							double initialPhase_tt = clusterPhase_3D.at(0).at(nIndex).at(mIndex);
							double initialPhase_tp = clusterPhase_3D.at(1).at(nIndex).at(mIndex);
							double initialPhase_pt = clusterPhase_3D.at(2).at(nIndex).at(mIndex);
							double initialPhase_pp = clusterPhase_3D.at(3).at(nIndex).at(mIndex);

							Vector2D rxRadiationField = rxAntenna->GetRadiationPattern_polar(rayZoa_radian[nIndex][mIndex], rayAoa_radian[nIndex][mIndex], uIndex);
							Vector2D txRadiationField = txAntenna->GetRadiationPattern_polar(rayZod_radian[nIndex][mIndex], rayAod_radian[nIndex][mIndex], sIndex);

							std::complex<double> initialPhase(0, 0);
							initialPhase = (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tt)) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pt)) / sqrt(Knm)) * txRadiationField.x + (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tp)) / sqrt(Knm) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pp))) * txRadiationField.y;

							//lambda_0 is accounted in the antenna spacing uLoc and sLoc.
							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * uLoc.x + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * uLoc.y + cos(rayZoa_radian[nIndex][mIndex]) * uLoc.z);
							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * sLoc.x + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * sLoc.y + cos(rayZod_radian[nIndex][mIndex]) * sLoc.z);
							//Doppler is computed in the CalBeamformingGain function and is simplified to only account for the center anngle of each cluster.
							//double doppler = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) + cos(rayZoa_radian[nIndex][mIndex]) * relativeSpeed.z * cos(velocity_angle.theta)) * slotTime * m_frequency / 3e8;
							rays += initialPhase *
									exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
							//* exp(std::complex<double>(0, doppler));
							//rays += 1;
						}
						//rays *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						rays *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = rays;
					}
					else //(7.5-28)
					{
						std::complex<double> raysSub1(0, 0);
						std::complex<double> raysSub2(0, 0);
						std::complex<double> raysSub3(0, 0);

						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{

							//ZML:Just remind me that the angle offsets for the 3 subclusters were not generated correctly.
							double initialPhase_tt = clusterPhase_3D.at(0).at(nIndex).at(mIndex);
							double initialPhase_tp = clusterPhase_3D.at(1).at(nIndex).at(mIndex);
							double initialPhase_pt = clusterPhase_3D.at(2).at(nIndex).at(mIndex);
							double initialPhase_pp = clusterPhase_3D.at(3).at(nIndex).at(mIndex);

							Vector2D rxRadiationField = rxAntenna->GetRadiationPattern_polar(rayZoa_radian[nIndex][mIndex], rayAoa_radian[nIndex][mIndex], uIndex);
							Vector2D txRadiationField = txAntenna->GetRadiationPattern_polar(rayZod_radian[nIndex][mIndex], rayAod_radian[nIndex][mIndex], sIndex);

							std::complex<double> initialPhase(0, 0);
							initialPhase = (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tt)) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pt)) / sqrt(Knm)) * txRadiationField.x + (rxRadiationField.x * exp(std::complex<double>(0, initialPhase_tp)) / sqrt(Knm) + rxRadiationField.y * exp(std::complex<double>(0, initialPhase_pp))) * txRadiationField.y;

							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * uLoc.x + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * uLoc.y + cos(rayZoa_radian[nIndex][mIndex]) * uLoc.z);
							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * sLoc.x + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * sLoc.y + cos(rayZod_radian[nIndex][mIndex]) * sLoc.z);
							//double doppler = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * relativeSpeed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) + cos(rayZoa_radian[nIndex][mIndex]) * relativeSpeed.z * cos(velocity_angle.theta)) * slotTime * m_frequency / 3e8;
							//double delaySpread;

							switch (mIndex)
							{
							case 9:
							case 10:
							case 11:
							case 12:
							case 17:
							case 18:
								//delaySpread= -2*M_PI*(clusterDelay.at(nIndex)+1.28*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub2 += initialPhase
											//*(rxAntenna->GetRadiationPattern(rayZoa_radian[nIndex][mIndex])*txAntenna->GetRadiationPattern(rayZod_radian[nIndex][mIndex]))
											* exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//* exp(std::complex<double>(0, doppler));
								//raysSub2 += 1;
								break;
							case 13:
							case 14:
							case 15:
							case 16:
								//delaySpread = -2*M_PI*(clusterDelay.at(nIndex)+2.56*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub3 += initialPhase
											//    *(rxAntenna->GetRadiationPattern(rayZoa_radian[nIndex][mIndex])*txAntenna->GetRadiationPattern(rayZod_radian[nIndex][mIndex]))
											* exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//* exp(std::complex<double>(0, doppler));
								//raysSub3 += 1;
								break;
							default: //case 1,2,3,4,5,6,7,8,19,20
								//delaySpread = -2*M_PI*clusterDelay.at(nIndex)*m_phyMacConfig->GetCenterFrequency ();
								raysSub1 += initialPhase
											//    *(rxAntenna->GetRadiationPattern(rayZoa_radian[nIndex][mIndex])*txAntenna->GetRadiationPattern(rayZod_radian[nIndex][mIndex]))
											* exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//* exp(std::complex<double>(0, doppler));
								//raysSub1 += 1;
								break;
							}
						}
						//raysSub1 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						//raysSub2 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						//raysSub3 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						raysSub1 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub2 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub3 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = raysSub1;
						H_usn.at(uIndex).at(sIndex).push_back(raysSub2);
						H_usn.at(uIndex).at(sIndex).push_back(raysSub3);
					}
				}
				if (params->m_los) //(7.5-29) && (7.5-30)
				{
					Vector2D rxRadiationField = rxAntenna->GetRadiationPattern_polar(rxAngle.theta, rxAngle.phi, uIndex);
					Vector2D txRadiationField = txAntenna->GetRadiationPattern_polar(txAngle.theta, txAngle.phi, sIndex);

					std::complex<double> initialPhase(0, 0);
					initialPhase = rxRadiationField.x * txRadiationField.x - rxRadiationField.y * txRadiationField.y;

					std::complex<double> ray(0, 0);
					double rxPhaseDiff = 2 * M_PI * (sin(rxAngle.theta) * cos(rxAngle.phi) * uLoc.x + sin(rxAngle.theta) * sin(rxAngle.phi) * uLoc.y + cos(rxAngle.theta) * uLoc.z);
					double txPhaseDiff = 2 * M_PI * (sin(txAngle.theta) * cos(txAngle.phi) * sLoc.x + sin(txAngle.theta) * sin(txAngle.phi) * sLoc.y + cos(txAngle.theta) * sLoc.z);
					//double doppler = 2*M_PI*(sin(rxAngle.theta)*cos(rxAngle.phi)*relativeSpeed.x
					//        + sin(rxAngle.theta)*sin(rxAngle.phi)*relativeSpeed.y
					//        + cos(rxAngle.theta)*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;
					//double doppler = 2 * M_PI * (sin(rxAngle.theta) * cos(rxAngle.phi) * relativeSpeed.x * (sin(velocity_angle.theta) * cos(velocity_angle.phi)) + sin(rxAngle.theta) * sin(rxAngle.phi) * relativeSpeed.y * (sin(velocity_angle.theta) * sin(velocity_angle.phi)) + cos(rxAngle.theta) * relativeSpeed.z * cos(velocity_angle.theta)) * slotTime * m_frequency / 3e8;
					double dis3D = params->m_dis3D;
					ray = //exp(std::complex<double>(0, losPhase)) doesnt use losphase in 38.901 (7.5-29)
						initialPhase * exp(std::complex<double>(0, -2 * M_PI * dis3D * m_frequency / 3e8)) *
						exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
					//* exp(std::complex<double>(0, doppler));

					double K_linear = pow(10, K_factor / 10);
					// the LOS path should be attenuated if blockage is enabled.
					H_usn.at(uIndex).at(sIndex).at(0) = sqrt(1 / (K_linear + 1)) * H_usn.at(uIndex).at(sIndex).at(0) + sqrt(K_linear / (1 + K_linear)) * ray / pow(10, attenuation_dB.at(0) / 10); //(7.5-30) for tau = tau1
					double tempSize = H_usn.at(uIndex).at(sIndex).size();

					for (uint8_t nIndex = 1; nIndex < tempSize; nIndex++)
					{
						H_usn.at(uIndex).at(sIndex).at(nIndex) *= sqrt(1 / (K_linear + 1)); //(7.5-30) for tau = tau2...taunN
					}
				}
			}
			else
			{
				for (uint8_t nIndex = 0; nIndex < params->m_numCluster; nIndex++)
				{
					//Compute the N-2 weakest cluster, only vertical polarization. (7.5-22)
					if (nIndex != cluster1st && nIndex != cluster2nd)
					{
						std::complex<double> rays(0, 0);
						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{

							double initialPhase = clusterPhase_2D.at(nIndex).at(mIndex);
							//lambda_0 is accounted in the antenna spacing uLoc and sLoc.
							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * uLoc.x + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * uLoc.y + cos(rayZoa_radian[nIndex][mIndex]) * uLoc.z);

							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * sLoc.x + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * sLoc.y + cos(rayZod_radian[nIndex][mIndex]) * sLoc.z);
							//Doppler is computed in the CalBeamformingGain function and is simplified to only account for the center anngle of each cluster.
							//double doppler = 2*M_PI*(sin(rayZoa_radian[nIndex][mIndex])*cos(rayAoa_radian[nIndex][mIndex])*relativeSpeed.x
							//		+ sin(rayZoa_radian[nIndex][mIndex])*sin(rayAoa_radian[nIndex][mIndex])*relativeSpeed.y
							//		+ cos(rayZoa_radian[nIndex][mIndex])*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;
							rays += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
							//*exp(std::complex<double>(0, doppler));
							//rays += 1;
						}
						//rays *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						rays *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = rays;
					}
					else //(7.5-28)
					{
						std::complex<double> raysSub1(0, 0);
						std::complex<double> raysSub2(0, 0);
						std::complex<double> raysSub3(0, 0);

						for (uint8_t mIndex = 0; mIndex < raysPerCluster; mIndex++)
						{

							//ZML:Just remind me that the angle offsets for the 3 subclusters were not generated correctly.

							double initialPhase = clusterPhase_2D.at(nIndex).at(mIndex);
							double rxPhaseDiff = 2 * M_PI * (sin(rayZoa_radian[nIndex][mIndex]) * cos(rayAoa_radian[nIndex][mIndex]) * uLoc.x + sin(rayZoa_radian[nIndex][mIndex]) * sin(rayAoa_radian[nIndex][mIndex]) * uLoc.y + cos(rayZoa_radian[nIndex][mIndex]) * uLoc.z);
							double txPhaseDiff = 2 * M_PI * (sin(rayZod_radian[nIndex][mIndex]) * cos(rayAod_radian[nIndex][mIndex]) * sLoc.x + sin(rayZod_radian[nIndex][mIndex]) * sin(rayAod_radian[nIndex][mIndex]) * sLoc.y + cos(rayZod_radian[nIndex][mIndex]) * sLoc.z);
							//double doppler = 2*M_PI*(sin(rayZoa_radian[nIndex][mIndex])*cos(rayAoa_radian[nIndex][mIndex])*relativeSpeed.x
							//		+ sin(rayZoa_radian[nIndex][mIndex])*sin(rayAoa_radian[nIndex][mIndex])*relativeSpeed.y
							//		+ cos(rayZoa_radian[nIndex][mIndex])*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;
							//double delaySpread;
							switch (mIndex)
							{
							case 9:
							case 10:
							case 11:
							case 12:
							case 17:
							case 18:
								//delaySpread= -2*M_PI*(clusterDelay.at(nIndex)+1.28*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub2 += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//*exp(std::complex<double>(0, doppler));
								//raysSub2 +=1;
								break;
							case 13:
							case 14:
							case 15:
							case 16:
								//delaySpread = -2*M_PI*(clusterDelay.at(nIndex)+2.56*c_DS)*m_phyMacConfig->GetCenterFrequency ();
								raysSub3 += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//*exp(std::complex<double>(0, doppler));
								//raysSub3 +=1;
								break;
							default: //case 1,2,3,4,5,6,7,8,19,20
								//delaySpread = -2*M_PI*clusterDelay.at(nIndex)*m_phyMacConfig->GetCenterFrequency ();
								raysSub1 += exp(std::complex<double>(0, initialPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rayZoa_radian[nIndex][mIndex]) * txAntenna->GetRadiationPattern_nonpolar(rayZod_radian[nIndex][mIndex])) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
								//*exp(std::complex<double>(0, doppler));
								//raysSub1 +=1;
								break;
							}
						}
						//raysSub1 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						//raysSub2 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						//raysSub3 *= sqrt(clusterPower.at(nIndex))/raysPerCluster;
						raysSub1 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub2 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						raysSub3 *= sqrt(clusterPower.at(nIndex) / raysPerCluster);
						H_usn.at(uIndex).at(sIndex).at(nIndex) = raysSub1;
						H_usn.at(uIndex).at(sIndex).push_back(raysSub2);
						H_usn.at(uIndex).at(sIndex).push_back(raysSub3);
					}
				}
				if (params->m_los) //(7.5-29) && (7.5-30)
				{
					std::complex<double> ray(0, 0);
					double rxPhaseDiff = 2 * M_PI * (sin(rxAngle.theta) * cos(rxAngle.phi) * uLoc.x + sin(rxAngle.theta) * sin(rxAngle.phi) * uLoc.y + cos(rxAngle.theta) * uLoc.z);
					double txPhaseDiff = 2 * M_PI * (sin(txAngle.theta) * cos(txAngle.phi) * sLoc.x + sin(txAngle.theta) * sin(txAngle.phi) * sLoc.y + cos(txAngle.theta) * sLoc.z);
					//double doppler = 2*M_PI*(sin(rxAngle.theta)*cos(rxAngle.phi)*relativeSpeed.x
					//		+ sin(rxAngle.theta)*sin(rxAngle.phi)*relativeSpeed.y
					//		+ cos(rxAngle.theta)*relativeSpeed.z)*slotTime*m_phyMacConfig->GetCenterFrequency ()/3e8;

					ray = exp(std::complex<double>(0, losPhase)) * (rxAntenna->GetRadiationPattern_nonpolar(rxAngle.theta) * txAntenna->GetRadiationPattern_nonpolar(txAngle.theta)) * exp(std::complex<double>(0, rxPhaseDiff)) * exp(std::complex<double>(0, txPhaseDiff));
					//*exp(std::complex<double>(0, doppler));

					double K_linear = pow(10, K_factor / 10);

					H_usn.at(uIndex).at(sIndex).at(0) = sqrt(1 / (K_linear + 1)) * H_usn.at(uIndex).at(sIndex).at(0) + sqrt(K_linear / (1 + K_linear)) * ray / pow(10, attenuation_dB.at(0) / 10); //(7.5-30) for tau = tau1
					double tempSize = H_usn.at(uIndex).at(sIndex).size();
					for (uint8_t nIndex = 1; nIndex < tempSize; nIndex++)
					{
						H_usn.at(uIndex).at(sIndex).at(nIndex) *= sqrt(1 / (K_linear + 1)); //(7.5-30) for tau = tau2...taunN
					}
				}
			}
		}
	}

	if (cluster1st == cluster2nd)
	{
		clusterDelay.push_back(clusterDelay.at(cluster2nd) + 1.28 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(cluster2nd) + 2.56 * table3gpp->m_cDS);

		clusterAoa.push_back(clusterAoa.at(cluster2nd));
		clusterAoa.push_back(clusterAoa.at(cluster2nd));
		clusterZoa.push_back(clusterZoa.at(cluster2nd));
		clusterZoa.push_back(clusterZoa.at(cluster2nd));
	}
	else
	{
		double min, max;
		if (cluster1st < cluster2nd)
		{
			min = cluster1st;
			max = cluster2nd;
		}
		else
		{
			min = cluster2nd;
			max = cluster1st;
		}
		clusterDelay.push_back(clusterDelay.at(min) + 1.28 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(min) + 2.56 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(max) + 1.28 * table3gpp->m_cDS);
		clusterDelay.push_back(clusterDelay.at(max) + 2.56 * table3gpp->m_cDS);

		clusterAoa.push_back(clusterAoa.at(min));
		clusterAoa.push_back(clusterAoa.at(min));
		clusterAoa.push_back(clusterAoa.at(max));
		clusterAoa.push_back(clusterAoa.at(max));

		clusterZoa.push_back(clusterZoa.at(min));
		clusterZoa.push_back(clusterZoa.at(min));
		clusterZoa.push_back(clusterZoa.at(max));
		clusterZoa.push_back(clusterZoa.at(max));
	}

	NS_LOG_INFO("size of coefficient matrix =[" << H_usn.size() << "][" << H_usn.at(0).size() << "][" << H_usn.at(0).at(0).size() << "]");

	/*std::cout << "Delay:";
	for (uint8_t i = 0; i < clusterDelay.size(); i++)
	{
		std::cout <<clusterDelay.at(i)<<"s\t";
	}
	std::cout << "\n";*/

	params->m_delay = clusterDelay;
	params->m_channel = H_usn;
	params->m_angle.clear();
	params->m_angle.push_back(clusterAoa);
	params->m_angle.push_back(clusterZoa);
	params->m_angle.push_back(clusterAod);
	params->m_angle.push_back(clusterZod);
	//update the previous location.

	return params;
}

void MmWave3gppChannel::BeamSearchBeamforming(Ptr<const SpectrumValue> txPsd, Ptr<Params3gpp> params, Ptr<AntennaArrayModel> txAntenna,
											  Ptr<AntennaArrayModel> rxAntenna, uint8_t *txAntennaNum, uint8_t *rxAntennaNum) const
{
	double max = 0, maxTx = 0, maxRx = 0, maxTxTheta = 0, maxRxTheta = 0;
	NS_LOG_UNCOND("BeamSearchBeamforming method at time " << Simulator::Now().GetSeconds());
	for (uint16_t txTheta = 60; txTheta < 121; txTheta = txTheta + 10)
	{
		for (uint16_t tx = 0; tx <= txAntennaNum[1]; tx++)
		{
			for (uint16_t rxTheta = 60; rxTheta < 121; rxTheta = rxTheta + 10)
			{
				for (uint16_t rx = 0; rx <= rxAntennaNum[1]; rx++)
				{
					NS_LOG_LOGIC("txTheta " << txTheta << " rxTheta " << rxTheta << " tx sector " << (M_PI * (double)tx / (double)txAntennaNum[1] - 0.5 * M_PI) / (M_PI)*180 << " rx sector " << (M_PI * (double)rx / (double)rxAntennaNum[1] - 0.5 * M_PI) / (M_PI)*180);

					txAntenna->SetSector(tx, txAntennaNum, txTheta);
					rxAntenna->SetSector(rx, rxAntennaNum, rxTheta);
					params->m_txW = txAntenna->GetBeamformingVector();
					params->m_rxW = rxAntenna->GetBeamformingVector();
					CalLongTerm(params);
					Ptr<SpectrumValue> bfPsd = CalBeamformingGain(txPsd, params, Vector(0, 0, 0));

					SpectrumValue bfGain = (*bfPsd) / (*txPsd);
					uint8_t nbands = bfGain.GetSpectrumModel()->GetNumBands();
					double power = Sum(bfGain) / nbands;

					NS_LOG_LOGIC("gain " << power);
					if (max < power)
					{
						max = power;
						maxTx = tx;
						maxRx = rx;
						maxTxTheta = txTheta;
						maxRxTheta = rxTheta;
					}
				}
			}
		}
	}
	NS_LOG_LOGIC("maxTx " << maxTx << " txAntennaNum[1] " << (uint16_t)txAntennaNum[1]);
	NS_LOG_LOGIC("max gain " << max << " maxTx " << (M_PI * (double)maxTx / (double)txAntennaNum[1] - 0.5 * M_PI) / (M_PI)*180 << " maxRx " << (M_PI * (double)maxRx / (double)rxAntennaNum[1] - 0.5 * M_PI) / (M_PI)*180 << " maxTxTheta " << maxTxTheta << " maxRxTheta " << maxRxTheta);
	txAntenna->SetSector(maxTx, txAntennaNum, maxTxTheta);
	rxAntenna->SetSector(maxRx, rxAntennaNum, maxRxTheta);
	params->m_txW = txAntenna->GetBeamformingVector();
	params->m_rxW = rxAntenna->GetBeamformingVector();
}

//180709-jskim14-add new function
void MmWave3gppChannel::IdealBeamforming(Ptr<const SpectrumValue> txPsd, Ptr<Params3gpp> params, Ptr<AntennaArrayModel> txAntenna,
										 Ptr<AntennaArrayModel> rxAntenna, uint8_t *txAntennaNum, uint8_t *rxAntennaNum) const
{
	complex2DVector_t txWeight = txAntenna->GetAntennaWeightMatrix();
	complex2DVector_t rxWeight = rxAntenna->GetAntennaWeightMatrix();
	NS_LOG_INFO("# of Tx antenna element: " << txWeight.size() << ", # of Tx beams: " << txWeight[0].size());
	NS_LOG_INFO("# of Rx antenna element: " << rxWeight.size() << ", # of Rx beams: " << rxWeight[0].size());
	uint16_t txBeamNum = txWeight[0].size();
	uint16_t rxBeamNum = rxWeight[0].size();

	// Vector txTxru = txAntenna->GetTxruNum();
	// Vector rxTxru = rxAntenna->GetTxruNum();

	double max = 0, maxTx = 0, maxRx = 0;
	for (uint16_t tx = 0; tx < txBeamNum; tx++)
	{
		complex2DVector_t txBSMat(txBeamNum, complexVector_t(1, 0)); //Initialize Tx beam selection matrix
		for (uint16_t cI = 0; cI < 1; cI++)
		{
			txBSMat[tx][cI] = 1;
		}
		for (uint16_t rx = 0; rx < rxBeamNum; rx++)
		{
			// uint16_t vTx = tx % (uint16_t)txTxru.x;
			// uint16_t hTx = tx / (uint16_t)txTxru.x;
			// uint16_t vRx = rx % (uint16_t)rxTxru.x;
			// uint16_t hRx = rx / (uint16_t)rxTxru.x;
			//hBeamInd*m_vTxruNum+vBeamInd
			//NS_LOG_INFO("Tx index=" << tx << ", elevation degree=" << (double)180 / (txTxru.x) * (vTx + 1) << ", azimuth degree=" << (double)360 / (txTxru.y * txTxru.z) * (hTx + 1) - 180 <<
			//		 	  ", Rx index=" << rx << ", elevation degree=" << (double)180 / (rxTxru.x) * (vRx + 1) << ", azimuth degree=" << (double)360 / (rxTxru.y * rxTxru.z) * (hRx + 1) - 180);
			NS_LOG_INFO("Tx elevation: index=" << tx << ", degree=" << (double)180 / txBeamNum * (tx + 1) << ", rx elevation: index=" << rx << ", degree=" << (double)180 / rxBeamNum * (rx + 1));

			complex2DVector_t rxBSMat(rxBeamNum, complexVector_t(1, 0)); //Initialize Rx beam selection matrix
			for (uint16_t cI = 0; cI < 1; cI++)
			{
				rxBSMat[rx][cI] = 1;
			}
			params->m_txWMat = Multiplication(txWeight, txBSMat);
			params->m_rxWMat = Multiplication(rxWeight, rxBSMat);

			CalLongTerm(params);
			Ptr<SpectrumValue> bfPsd = CalBeamformingGain(txPsd, params, Vector(0, 0, 0));
			SpectrumValue bfGain = (*bfPsd) / (*txPsd);
			uint8_t nbands = bfGain.GetSpectrumModel()->GetNumBands();
			double power = Sum(bfGain) / nbands;
			double priorPower = Sum(*txPsd) / nbands;
			double afterPower = Sum(*bfPsd) / nbands;

			NS_LOG_INFO("Gain=" << power << ", Prior power=" << priorPower << ", After power=" << afterPower << ", nbands=" << (unsigned)nbands);

			if (max < power)
			{
				max = power;
				maxTx = tx;
				maxRx = rx;
			}
		}
	}

	// uint16_t vTx = (uint16_t)maxTx % (uint16_t)txTxru.x;
	// uint16_t hTx = (uint16_t)maxTx / (uint16_t)txTxru.x;
	// uint16_t vRx = (uint16_t)maxRx % (uint16_t)rxTxru.x;
	// uint16_t hRx = (uint16_t)maxRx / (uint16_t)rxTxru.x;
	// NS_LOG_UNCOND("Max Tx index=" << maxTx << ", elevation degree=" << (double)180 / (txTxru.x) * (vTx + 1) << ", azimuth degree=" << (double)120 / (txTxru.y * txTxru.z) * (hTx + 1) - 120 <<
	//			  ", Max Rx index=" << maxRx << ", elevation degree=" << (double)180 / (rxTxru.x) * (vRx + 1) << ", azimuth degree=" << (double)120 / (rxTxru.y * rxTxru.z) * (hRx + 1) - 120);
	NS_LOG_UNCOND("maxTx elevation: index=" << maxTx << ", degree=" << (double)180 / txBeamNum * (maxTx + 1) << ", maxRx elevation: index=" << maxRx << ", degree=" << (double)180 / rxBeamNum * (maxRx + 1));
	NS_LOG_UNCOND("Gain [dB]=" << 10 * std::log10(max));
	complex2DVector_t maxTxBSMat(txBeamNum, complexVector_t(1, 0)); //Initialize Tx beam selection matrix
	for (uint16_t cI = 0; cI < 1; cI++)
	{
		maxTxBSMat[maxTx][cI] = 1;
	}
	complex2DVector_t maxRxBSMat(rxBeamNum, complexVector_t(1, 0)); //Initialize Rx beam selection matrix
	for (uint16_t cI = 0; cI < 1; cI++)
	{
		maxRxBSMat[maxRx][cI] = 1;
	}
	params->m_txWMat = Multiplication(txWeight, maxTxBSMat);
	params->m_rxWMat = Multiplication(rxWeight, maxRxBSMat);
}

complex2DVector_t
MmWave3gppChannel::Multiplication(complex2DVector_t mtx1, complex2DVector_t mtx2) const
{
	int r1 = mtx1.size();
	int c1 = mtx1[0].size();
	int r2 = mtx2.size();
	int c2 = mtx2[0].size();

	if (c1 != r2)
	{
		NS_FATAL_ERROR("Matrix multiplication error !");
	}

	complex2DVector_t mtx3(r1, complexVector_t(c2, 0));
	for (int i = 0; i < r1; i++)
	{
		for (int j = 0; j < c2; j++)
		{
			mtx3[i][j] = 0;
			for (int k = 0; k < r2; k++)
			{
				mtx3[i][j] += mtx1[i][k] * mtx2[k][j];
			}
		}
	}
	return mtx3;
}

complex2DVector_t
MmWave3gppChannel::Transpose(complex2DVector_t mtx)
{
	int r = mtx.size();
	int c = mtx[0].size();

	complex2DVector_t temp(c, std::vector<std::complex<double>>(r, 0));

	for (int i = 0; i < r; i++)
	{
		for (int j = 0; j < c; j++)
		{
			temp[j][i] = mtx[i][j];
		}
	}
	return temp;
}

std::complex<double>
MmWave3gppChannel::Determinant(complex2DVector_t mtx, int size)
{
	std::complex<double> det = std::complex<double>(0, 0);

	if (size < 1)
	{ /* Error */
	}
	else if (size == 1)
	{ /* Shouldn't get used */
		det = mtx[0][0];
	}
	else if (size == 2)
	{
		det = mtx[0][0] * mtx[1][1] - mtx[1][0] * mtx[0][1];
	}
	else
	{
		int i, j, j1, j2;
		complex2DVector_t m(size, complexVector_t(size, 0));
		for (j1 = 0; j1 < size; j1++)
		{
			for (i = 1; i < size; i++)
			{
				j2 = 0;
				for (j = 0; j < size; j++)
				{
					if (j == j1)
						continue;
					m[i - 1][j2] = mtx[i][j];
					j2++;
				}
			}
			det += std::pow(-1.0, j1 + 2.0) * mtx[0][j1] * Determinant(m, size - 1);
		}
	}
	//NS_LOG_ERROR("determinant: "<<det);
	//NS_LOG_ERROR("mtx: "<<mtx[0][0]<<"  "<<mtx[0][1]<<"  "<<mtx[1][0]<<"  "<<mtx[1][1]);
	return det;
}

complex2DVector_t
MmWave3gppChannel::Inverse(complex2DVector_t mtx)
{
	int i, j, ii, jj, i1, j1;
	std::complex<double> det;

	int size = mtx.size();
	complex2DVector_t c(size, complexVector_t(size, 0));
	complex2DVector_t cmtx(size, complexVector_t(size, 0));
	complex2DVector_t temp(size, complexVector_t(size, 0));

	for (j = 0; j < size; j++)
	{
		for (i = 0; i < size; i++)
		{
			/* Form the adjoint a_ij */
			i1 = 0;
			for (ii = 0; ii < size; ii++)
			{
				if (ii == i)
					continue;
				j1 = 0;
				for (jj = 0; jj < size; jj++)
				{
					if (jj == j)
						continue;
					c[i1][j1] = mtx[ii][jj];
					j1++;
				}
				i1++;
			}

			/* Calculate the determinate */
			det = Determinant(c, size - 1);

			/* Fill in the elements of the cofactor */
			cmtx[i][j] = std::pow(-1.0, i + j + 2.0) * det;
		}
	}
	temp = Transpose(cmtx);
	std::complex<double> determinant = Determinant(mtx, size);
	for (int i = 0; i < size; i++)
		for (int j = 0; j < size; j++)
			temp[i][j] /= determinant;

	return temp;
}

complex2DVector_t
MmWave3gppChannel::Hermitian(complex2DVector_t mtx)
{
	complex2DVector_t temp = Transpose(mtx);
	int r = temp.size();
	int c = temp[0].size();

	for (int i = 0; i < r; i++)
	{
		for (int j = 0; j < c; j++)
		{
			temp[i][j] = std::conj(temp[i][j]);
		}
	}
	return temp;
}
//jskim14-end

doubleVector_t
MmWave3gppChannel::CalAttenuationOfBlockage(Ptr<Params3gpp> params,
											doubleVector_t clusterAOA, doubleVector_t clusterZOA) const
{
	doubleVector_t powerAttenuation;
	uint8_t clusterNum = clusterAOA.size();
	for (uint8_t cInd = 0; cInd < clusterNum; cInd++)
	{
		powerAttenuation.push_back(0); //Initial power attenuation for all clusters to be 0 dB;
	}
	//step a: the number of non-self blocking blockers is stored in m_numNonSelfBloking.

	//step b:Generate the size and location of each blocker
	//generate self blocking (i.e., for blockage from the human body)
	double phi_sb, x_sb, theta_sb, y_sb;
	//table 7.6.4.1-1 Self-blocking region parameters.
	if (m_portraitMode)
	{
		phi_sb = 260;
		x_sb = 120;
		theta_sb = 100;
		y_sb = 80;
	}
	else // landscape mode
	{
		phi_sb = 40;
		x_sb = 160;
		theta_sb = 110;
		y_sb = 75;
	}

	//generate or update non-self blocking
	if (params->m_nonSelfBlocking.size() == 0) //generate new blocking regions
	{
		for (uint16_t blockInd = 0; blockInd < m_numNonSelfBloking; blockInd++)
		{
			//draw value from table 7.6.4.1-2 Blocking region parameters
			doubleVector_t table;
			table.push_back(m_normalRvBlockage->GetValue()); //phi_k: store the normal RV that will be mapped to uniform (0,360) later.
			if (m_scenario == "InH-OfficeMixed" || m_scenario == "InH-OfficeOpen")
			{
				table.push_back(m_uniformRvBlockage->GetValue(15, 45)); //x_k
				table.push_back(90);									//Theta_k
				table.push_back(m_uniformRvBlockage->GetValue(5, 15));  //y_k
				table.push_back(2);										//r
			}
			else
			{
				table.push_back(m_uniformRvBlockage->GetValue(5, 15)); //x_k
				table.push_back(90);								   //Theta_k
				table.push_back(5);									   //y_k
				table.push_back(10);								   //r
			}
			params->m_nonSelfBlocking.push_back(table);
		}
	}
	else
	{
		double deltaX = sqrt(pow(params->m_preLocUT.x - params->m_locUT.x, 2) + pow(params->m_preLocUT.y - params->m_locUT.y, 2));
		//if deltaX and speed are both 0, the autocorrelation is 1, skip updating
		if (deltaX > 1e-6 || m_blockerSpeed > 1e-6)
		{
			double corrDis;
			//draw value from table 7.6.4.1-4: Spatial correlation distance for different scenarios.
			if (m_scenario == "InH-OfficeMixed" || m_scenario == "InH-OfficeOpen")
			{
				//InH, correlation distance = 5;
				corrDis = 5;
			}
			else
			{
				if (params->m_o2i) // outdoor to indoor
				{
					corrDis = 5;
				}
				else //LOS or NLOS
				{
					corrDis = 10;
				}
			}
			double R;
			if (m_blockerSpeed > 1e-6) // speed not equal to 0
			{
				double corrT = corrDis / m_blockerSpeed;
				R = exp(-1 * (deltaX / corrDis + (Now().GetSeconds() - params->m_generatedTime.GetSeconds()) / corrT));
			}
			else
			{
				R = exp(-1 * (deltaX / corrDis));
			}

			NS_LOG_INFO("Distance change:" << deltaX << " Speed:" << m_blockerSpeed
										   << " Time difference:" << Now().GetSeconds() - params->m_generatedTime.GetSeconds()
										   << " correlation:" << R);

			//In order to generate correlated uniform random variables, we first generate correlated normal random variables and map the normal RV to uniform RV.
			//Notice the correlation will change if the RV is transformed from normal to uniform.
			//To compensate the distortion, the correlation of the normal RV is computed
			//such that the uniform RV would have the desired correlation when transformed from normal RV.

			//The following formula was obtained from MATLAB numerical simulation.

			if (R * R * (-0.069) + R * 1.074 - 0.002 < 1) //transform only when the correlation of normal RV is smaller than 1
			{
				R = R * R * (-0.069) + R * 1.074 - 0.002;
			}
			for (uint16_t blockInd = 0; blockInd < m_numNonSelfBloking; blockInd++)
			{

				//Generate a new correlated normal RV with the following formula
				params->m_nonSelfBlocking.at(blockInd).at(PHI_INDEX) =
					R * params->m_nonSelfBlocking.at(blockInd).at(PHI_INDEX) + sqrt(1 - R * R) * m_normalRvBlockage->GetValue();
			}
		}
	}

	//step c: Determine the attenuation of each blocker due to blockers
	for (uint8_t cInd = 0; cInd < clusterNum; cInd++)
	{
		NS_ASSERT_MSG(clusterAOA.at(cInd) >= 0 && clusterAOA.at(cInd) <= 360, "the AOA should be the range of [0,360]");
		NS_ASSERT_MSG(clusterZOA.at(cInd) >= 0 && clusterZOA.at(cInd) <= 180, "the ZOA should be the range of [0,180]");

		//check self blocking
		NS_LOG_INFO("AOA=" << clusterAOA.at(cInd) << " Block Region[" << phi_sb - x_sb / 2 << "," << phi_sb + x_sb / 2 << "]");
		NS_LOG_INFO("ZOA=" << clusterZOA.at(cInd) << " Block Region[" << theta_sb - y_sb / 2 << "," << theta_sb + y_sb / 2 << "]");
		if (std::abs(clusterAOA.at(cInd) - phi_sb) < (x_sb / 2) && std::abs(clusterZOA.at(cInd) - theta_sb) < (y_sb / 2))
		{
			powerAttenuation.at(cInd) += 30; //anttenuate by 30 dB.
			NS_LOG_INFO("Cluster[" << (int)cInd << "] is blocked by self blocking region and reduce 30 dB power,"
												   "the attenuation is ["
								   << powerAttenuation.at(cInd) << " dB]");
		}

		//check non-self blocking
		double phiK, xK, thetaK, yK;
		for (uint16_t blockInd = 0; blockInd < m_numNonSelfBloking; blockInd++)
		{
			//The normal RV is transformed to uniform RV with the desired correlation.
			phiK = (0.5 * erfc(-1 * params->m_nonSelfBlocking.at(blockInd).at(PHI_INDEX) / sqrt(2))) * 360;
			while (phiK > 360)
			{
				phiK -= 360;
			}

			while (phiK < 0)
			{
				phiK += 360;
			}

			xK = params->m_nonSelfBlocking.at(blockInd).at(X_INDEX);
			thetaK = params->m_nonSelfBlocking.at(blockInd).at(THETA_INDEX);
			yK = params->m_nonSelfBlocking.at(blockInd).at(Y_INDEX);
			NS_LOG_INFO("AOA=" << clusterAOA.at(cInd) << " Block Region[" << phiK - xK << "," << phiK + xK << "]");
			NS_LOG_INFO("ZOA=" << clusterZOA.at(cInd) << " Block Region[" << thetaK - yK << "," << thetaK + yK << "]");

			if (std::abs(clusterAOA.at(cInd) - phiK) < (xK) && std::abs(clusterZOA.at(cInd) - thetaK) < (yK))
			{
				double A1 = clusterAOA.at(cInd) - (phiK + xK / 2);   //(7.6-24)
				double A2 = clusterAOA.at(cInd) - (phiK - xK / 2);   //(7.6-25)
				double Z1 = clusterZOA.at(cInd) - (thetaK + yK / 2); //(7.6-26)
				double Z2 = clusterZOA.at(cInd) - (thetaK - yK / 2); //(7.6-27)
				int signA1, signA2, signZ1, signZ2;
				//draw sign for the above parameters according to table 7.6.4.1-3 Description of signs
				if (xK / 2 < clusterAOA.at(cInd) - phiK && clusterAOA.at(cInd) - phiK <= xK)
				{
					signA1 = -1;
				}
				else
				{
					signA1 = 1;
				}
				if (-1 * xK < clusterAOA.at(cInd) - phiK && clusterAOA.at(cInd) - phiK <= -1 * xK / 2)
				{
					signA2 = -1;
				}
				else
				{
					signA2 = 1;
				}

				if (yK / 2 < clusterZOA.at(cInd) - thetaK && clusterZOA.at(cInd) - thetaK <= yK)
				{
					signZ1 = -1;
				}
				else
				{
					signZ1 = 1;
				}
				if (-1 * yK < clusterZOA.at(cInd) - thetaK && clusterZOA.at(cInd) - thetaK <= -1 * yK / 2)
				{
					signZ2 = -1;
				}
				else
				{
					signZ2 = 1;
				}
				double lambda = 3e8 / m_phyMacConfig->GetCenterFrequency();
				double F_A1 = atan(signA1 * M_PI / 2 * sqrt(M_PI / lambda * params->m_nonSelfBlocking.at(blockInd).at(R_INDEX) * (1 / cos(A1 * M_PI / 180) - 1))) / M_PI; //(7.6-23)
				double F_A2 = atan(signA2 * M_PI / 2 * sqrt(M_PI / lambda * params->m_nonSelfBlocking.at(blockInd).at(R_INDEX) * (1 / cos(A2 * M_PI / 180) - 1))) / M_PI;
				double F_Z1 = atan(signZ1 * M_PI / 2 * sqrt(M_PI / lambda * params->m_nonSelfBlocking.at(blockInd).at(R_INDEX) * (1 / cos(Z1 * M_PI / 180) - 1))) / M_PI;
				double F_Z2 = atan(signZ2 * M_PI / 2 * sqrt(M_PI / lambda * params->m_nonSelfBlocking.at(blockInd).at(R_INDEX) * (1 / cos(Z2 * M_PI / 180) - 1))) / M_PI;
				double L_dB = -20 * log10(1 - (F_A1 + F_A2) * (F_Z1 + F_Z2)); //(7.6-22)
				powerAttenuation.at(cInd) += L_dB;
				NS_LOG_INFO("Cluster[" << (int)cInd << "] is blocked by no-self blocking, "
													   "the loss is ["
									   << L_dB << "]"
									   << " dB");
			}
		}
	}
	return powerAttenuation;
}

} // namespace ns3
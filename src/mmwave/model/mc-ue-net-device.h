/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/* *
 * Copyright (c) 2016, University of Padova, Dep. of Information Engineering, SIGNET lab. 
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
 * Author: Michele Polese <michele.polese@gmail.com>
 * 
 */

 

#ifndef MC_UE_NET_DEVICE_H
#define MC_UE_NET_DEVICE_H

#include <ns3/net-device.h>
#include <ns3/event-id.h>
#include <ns3/mac48-address.h>
#include <ns3/traced-callback.h>
#include <ns3/nstime.h>
#include "ns3/lte-ue-mac.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/lte-ue-phy.h"
#include "ns3/lte-phy.h"
#include "ns3/epc-ue-nas.h"
#include "ns3/mmwave-ue-mac.h"
#include "ns3/mmwave-ue-phy.h"
//#include "ns3/mmwave-enb-net-device.h"
//#include "ns3/lte-enb-net-device.h"
//#include "ns3/lte-ue-net-device.h"
//#include "ns3/mmwave-ue-net-device.h"


namespace ns3 {

class Packet;
class PacketBurst;
class Node;
class LteEnbNetDevice;
class MmWaveEnbNetDevice;

/*
//180704-jskim14-add antenna parameters
struct AntennaParams 
{
    AntennaParams (): m_vAntennaNum (4), m_hAntennaNum (4), m_polarNum (1), m_vTxrusNum(2), m_hTxrusNum(4), m_connectMode(0)
	{
	}

	AntennaParams (uint8_t vAntennaNum, uint8_t hAntennaNum, uint8_t polarNum, uint8_t vTxrusNum, uint8_t hTxrusNum, uint8_t connectMode)
		: m_vAntennaNum (vAntennaNum), m_hAntennaNum (hAntennaNum), m_polarNum (polarNum), m_vTxrusNum (vTxrusNum), m_hTxrusNum (hTxrusNum), m_connectMode (connectMode)
	{
	}
  	uint8_t m_vAntennaNum; //The number of vertical antenna elements
   	uint8_t m_hAntennaNum; //The number of horizontal antenna elements
	uint8_t m_polarNum;    //The number of polarization dimension
   	uint8_t m_vTxrusNum;   //The number of vertical TXRUs
   	uint8_t m_hTxrusNum;   //The number of horizontal TXRUs
    uint8_t m_connectMode; //Antenna connection mode (0:1-D full, 1:2-D full, 2,3)
};
//jskim14-end
*/

/**
  * \ingroup mmWave
  * This class represents a MC LTE + mmWave UE NetDevice, therefore
  * it is a union of the UeNetDevice classes of those modules, 
  * up to some point
  */
class McUeNetDevice : public NetDevice
{
public: 
	// methods inherited from NetDevide. 
	// TODO check if 2 (or more) Mac Addresses are needed or if the 
	// same can be used for the 2 (or more) eNB

	static TypeId GetTypeId (void);

	McUeNetDevice ();
	virtual ~McUeNetDevice ();

    virtual void DoDispose (void);

    virtual void SetIfIndex (const uint32_t index);
    virtual uint32_t GetIfIndex (void) const;
    virtual Ptr<Channel> GetChannel (void) const;
    virtual void SetAddress (Address address);
    virtual Address GetAddress (void) const;
    virtual bool SetMtu (const uint16_t mtu);
    virtual uint16_t GetMtu (void) const;
    virtual bool IsLinkUp (void) const;
    virtual void AddLinkChangeCallback (Callback<void> callback);
    virtual bool IsBroadcast (void) const;
    virtual Address GetBroadcast (void) const;
    virtual bool IsMulticast (void) const;
    virtual Address GetMulticast (Ipv4Address multicastGroup) const;
    virtual bool IsBridge (void) const;
    virtual bool IsPointToPoint (void) const;
    virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
    virtual Ptr<Node> GetNode (void) const;
    virtual void SetNode (Ptr<Node> node);
    virtual bool NeedsArp (void) const;
    virtual Address GetMulticast (Ipv6Address addr) const;
    virtual void SetReceiveCallback (ReceiveCallback cb);
    virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);
    virtual bool SupportsSendFrom (void) const;
    virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);

    Ipv4Address GetPacketDestination (Ptr<Packet> packet);
   // bool IsAdditionalEnb;
  
  /** 
   * receive a packet from the lower layers in order to forward it to the upper layers
   * 
   * \param p the packet
   */
    void Receive (Ptr<Packet> p);

    // ---------------------------- Common ----------------------------
    /**
	* \brief Returns the CSG ID the UE is currently a member of.
	* \return the Closed Subscriber Group identity
	*/
	uint32_t GetCsgId () const;

	/**
	* \brief Enlist the UE device as a member of a particular CSG.
	* \param csgId the intended Closed Subscriber Group identity
	*
	* UE is associated with a single CSG identity, and thus becoming a member of
	* this particular CSG. As a result, the UE may gain access to cells which
	* belong to this CSG. This does not revoke the UE's access to non-CSG cells.
	*
	* \note This restriction only applies to initial cell selection and
	*       EPC-enabled simulation.
	*/
	void SetCsgId (uint32_t csgId);


	Ptr<EpcUeNas> GetNas (void) const;


	uint64_t GetImsi () const;


    // -------------------------- LTE methods -------------------------
    Ptr<LteUeMac> GetLteMac (void) const;

	Ptr<LteUeRrc> GetLteRrc () const;

	Ptr<LteUePhy> GetLtePhy (void) const;

	/**
	* \return the downlink carrier frequency (EARFCN)
	*
	* Note that real-life handset typically supports more than one EARFCN, but
	* the sake of simplicity we assume only one EARFCN is supported.
	*/
	uint16_t GetLteDlEarfcn () const;

	/**
	* \param earfcn the downlink carrier frequency (EARFCN)
	*
	* Note that real-life handset typically supports more than one EARFCN, but
	* the sake of simplicity we assume only one EARFCN is supported.
	*/
	void SetLteDlEarfcn (uint16_t earfcn);

	/**
	* \brief Set the targer eNB where the UE is registered
	* \param enb
	*/
	void SetLteTargetEnb (Ptr<LteEnbNetDevice> enb);

	/**
	* \brief Get the targer eNB where the UE is registered
	* \return the pointer to the enb
	*/
	Ptr<LteEnbNetDevice> GetLteTargetEnb (void);

	// ---------------------------- From mmWave ------------------------

	Ptr<MmWaveUePhy> GetMmWavePhy (void) const;

	Ptr<MmWaveUeMac> GetMmWaveMac (void) const;

	Ptr<LteUeRrc> GetMmWaveRrc () const;

	Ptr<MmWaveUePhy> GetMmWavePhy_2 (void) const;

	Ptr<MmWaveUeMac> GetMmWaveMac_2 (void) const;

	Ptr<LteUeRrc> GetMmWaveRrc_2 () const;

	uint16_t GetMmWaveEarfcn () const;

	void SetMmWaveEarfcn (uint16_t earfcn);

	void SetMmWaveTargetEnb (Ptr<MmWaveEnbNetDevice> enb);
	void SetMmWaveTargetEnb_2 (Ptr<MmWaveEnbNetDevice> enb); //sjkang1117


	Ptr<MmWaveEnbNetDevice> GetMmWaveTargetEnb (void);
	Ptr<MmWaveEnbNetDevice> GetMmWaveTargetEnb_2 (void); //sjkang1117

    void SetAntennaNum (uint8_t antennaNum);

    uint8_t GetAntennaNum () const;

	//180704-jskim14-add new functions
	void SetAntennaParams (uint8_t vAntennaNum, uint8_t hAntennaNum, uint8_t polarNum, uint8_t vTxruNum, uint8_t hTxruNum, uint8_t connectMode);
	void SetAntennaRotation (double alpha, double beta, double gamma, double pol); //180716-jskim14-input is degree
	uint8_t GetVAntennaNum ();
	uint8_t GetHAntennaNum ();
	uint8_t GetPolarNum ();
	uint8_t GetVTxruNum ();
	uint8_t GetHTxruNum ();
	uint8_t GetConnectMode ();
	Vector GetRotation (); //180716-jskim14
	double GetPolarization (); //180716-jskim14
    //jskim14-end

protected:
    NetDevice::ReceiveCallback m_rxCallback;
    virtual void DoInitialize (void);

private:
	
    Mac48Address m_macaddress;
    Ptr<Node> m_node;
    mutable uint16_t m_mtu;
    bool m_linkUp;
    uint32_t m_ifIndex;

    // From LTE
    bool m_isConstructed;
	TracedCallback<> m_linkChangeCallbacks;

	/**
	* \brief Propagate attributes and configuration to sub-modules.
	*
	* Several attributes (e.g., the IMSI) are exported as the attributes of the
	* McUeNetDevice from a user perspective, but are actually used also in other
	* sub-modules (the RRC, the PHY, etc.). This method takes care of updating
	* the configuration of all these sub-modules so that their copy of attribute
	* values are in sync with the one in the McUeNetDevice.
	*/
	void UpdateConfig ();

	bool DoSend (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);

	// LTE

	Ptr<LteEnbNetDevice> m_lteTargetEnb;
	Ptr<LteUeMac> m_lteMac;
	Ptr<LteUePhy> m_ltePhy;
	Ptr<LteUeRrc> m_lteRrc;
	uint16_t m_lteDlEarfcn; /**< LTE downlink carrier frequency */

	// MmWave
	Ptr<MmWaveEnbNetDevice> m_mmWaveTargetEnb;
	Ptr<MmWaveEnbNetDevice> m_mmWaveTargetEnb_2; //sjkang1117

	Ptr<MmWaveUePhy> m_mmWavePhy;
	Ptr<MmWaveUePhy> m_mmWavePhy_2; //sjkang

	Ptr<MmWaveUeMac> m_mmWaveMac;
	Ptr<MmWaveUeMac> m_mmWaveMac_2;  //sjkang
	Ptr<LteUeRrc> m_mmWaveRrc; // TODO consider a lightweight RRC for the mmwave part
	Ptr<LteUeRrc> m_mmWaveRrc_2;  //sjkang

	uint16_t m_mmWaveEarfcn; /**< MmWave carrier frequency */
	uint8_t m_mmWaveAntennaNum;

	// Common
	Ptr<EpcUeNas> m_nas;
	uint64_t m_imsi; 
	uint32_t m_csgId; 
	
	// TODO this will be useless
	Ptr<UniformRandomVariable> m_random;

	AntennaParams m_antennaParams; //180714-jskim14-Parameters of antenna
	//180716-jskim14-antenna rotation parameterd
	Vector m_rotation;
	double m_pol;
	//jskim14-end
};

} // namespace ns3

#endif 
//MC_UE_NET_DEVICE_H

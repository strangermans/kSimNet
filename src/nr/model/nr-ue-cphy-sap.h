/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011, 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2016, University of Padova, Dep. of Information Engineering, SIGNET lab
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
 * Author: Nicola Baldo <nbaldo@cttc.es>,
 *         Marco Miozzo <mmiozzo@cttc.es>
 *
 * Modified by: Michele Polese <michele.polese@gmail.com>
 *          Dual Connectivity functionalities
 */

#ifndef NR_UE_CPHY_SAP_H
#define NR_UE_CPHY_SAP_H

#include <stdint.h>
#include <ns3/ptr.h>

#include <ns3/nr-rrc-sap.h>

namespace ns3 {


class NrEnbNetDevice;

/**
 * Service Access Point (SAP) offered by the UE PHY to the UE RRC for control purposes
 *
 * This is the PHY SAP Provider, i.e., the part of the SAP that contains
 * the PHY methods called by the MAC
 */
class NrUeCphySapProvider
{
public:

  /** 
   * destructor
   */
  virtual ~NrUeCphySapProvider ();

  /** 
   * reset the PHY
   * 
   */
  virtual void Reset () = 0;

  /**
   * \brief Tell the PHY entity to listen to PSS from surrounding cells and
   *        measure the RSRP.
   * \param dlEarfcn the downlink carrier frequency (EARFCN) to listen to
   *
   * This function will instruct this PHY instance to listen to the DL channel
   * over the bandwidth of 6 RB at the frequency associated with the given
   * EARFCN.
   *
   * After this, it will start receiving Primary Synchronization Signal (PSS)
   * and periodically returning measurement reports to RRC via
   * NrUeCphySapUser::ReportUeMeasurements function.
   */
  virtual void StartCellSearch (uint16_t dlEarfcn) = 0;

  /**
   * \brief Tell the PHY entity to synchronize with a given eNodeB over the
   *        currently active EARFCN for communication purposes.
   * \param cellId the ID of the eNodeB to synchronize with
   *
   * By synchronizing, the PHY will start receiving various information
   * transmitted by the eNodeB. For instance, when receiving system information,
   * the message will be relayed to RRC via
   * NrUeCphySapUser::RecvMasterInformationBlock and
   * NrUeCphySapUser::RecvSystemInformationBlockType1 functions.
   *
   * Initially, the PHY will be configured to listen to 6 RBs of BCH.
   * NrUeCphySapProvider::SetDlBandwidth can be called afterwards to increase
   * the bandwidth.
   */
  virtual void SynchronizeWithEnb (uint16_t cellId) = 0;

  /**
   * \brief Tell the PHY entity to align to the given EARFCN and synchronize
   *        with a given eNodeB for communication purposes.
   * \param cellId the ID of the eNodeB to synchronize with
   * \param dlEarfcn the downlink carrier frequency (EARFCN)
   *
   * By synchronizing, the PHY will start receiving various information
   * transmitted by the eNodeB. For instance, when receiving system information,
   * the message will be relayed to RRC via
   * NrUeCphySapUser::RecvMasterInformationBlock and
   * NrUeCphySapUser::RecvSystemInformationBlockType1 functions.
   *
   * Initially, the PHY will be configured to listen to 6 RBs of BCH.
   * NrUeCphySapProvider::SetDlBandwidth can be called afterwards to increase
   * the bandwidth.
   */
  virtual void SynchronizeWithEnb (uint16_t cellId, uint16_t dlEarfcn) = 0;

  /**
   * \param dlBandwidth the DL bandwidth in number of PRBs
   */
  virtual void SetDlBandwidth (uint8_t dlBandwidth) = 0;

  /** 
   * \brief Configure uplink (normally done after reception of SIB2)
   * 
   * \param ulEarfcn the uplink carrier frequency (EARFCN)
   * \param ulBandwidth the UL bandwidth in number of PRBs
   */
  virtual void ConfigureUplink (uint16_t ulEarfcn, uint8_t ulBandwidth) = 0;

  /**
   * \brief Configure referenceSignalPower
   *
   * \param referenceSignalPower received from eNB in SIB2
   */
  virtual void ConfigureReferenceSignalPower (int8_t referenceSignalPower) = 0;

  /** 
   * 
   * \param rnti the cell-specific UE identifier
   */
  virtual void SetRnti (uint16_t rnti) = 0;

  /**
   * \param txMode the transmissionMode of the user
   */
  virtual void SetTransmissionMode (uint8_t txMode) = 0;

  /**
   * \param srcCi the SRS configuration index
   */
  virtual void SetSrsConfigurationIndex (uint16_t srcCi) = 0;

  /**
   * \param pa the P_A value
   */
  virtual void SetPa (double pa) = 0;

};


/**
 * Service Access Point (SAP) offered by the UE PHY to the UE RRC for control purposes
 *
 * This is the CPHY SAP User, i.e., the part of the SAP that contains the RRC
 * methods called by the PHY
*/
class NrUeCphySapUser
{
public:

  /** 
   * destructor
   */
  virtual ~NrUeCphySapUser ();


  /**
   * Parameters of the ReportUeMeasurements primitive: RSRP [dBm] and RSRQ [dB]
   * See section 5.1.1 and 5.1.3 of TS 36.214
   */
  struct UeMeasurementsElement
  {
    uint16_t m_cellId;
    double m_rsrp;  // [dBm]
    double m_rsrq;  // [dB]
  };

  struct UeMeasurementsParameters
  {
    std::vector <struct UeMeasurementsElement> m_ueMeasurementsList;
  };


  /**
   * \brief Relay an MIB message from the PHY entity to the RRC layer.
   * \param cellId the ID of the eNodeB where the message originates from
   * \param mib the Master Information Block message
   * 
   * This function is typically called after PHY receives an MIB message over
   * the BCH.
   */
  virtual void RecvMasterInformationBlock (uint16_t cellId,
                                           NrRrcSap::MasterInformationBlock mib) = 0;

  /**
   * \brief Relay an SIB1 message from the PHY entity to the RRC layer.
   * \param cellId the ID of the eNodeB where the message originates from
   * \param sib1 the System Information Block Type 1 message
   *
   * This function is typically called after PHY receives an SIB1 message over
   * the BCH.
   */
  virtual void RecvSystemInformationBlockType1 (uint16_t cellId,
                                                NrRrcSap::SystemInformationBlockType1 sib1) = 0;

  /**
   * \brief Send a report of RSRP and RSRQ values perceived from PSS by the PHY
   *        entity (after applying layer-1 filtering) to the RRC layer.
   * \param params the structure containing a vector of cellId, RSRP and RSRQ
   */
  virtual void ReportUeMeasurements (UeMeasurementsParameters params) = 0;

  virtual void NotifyRadioLinkFailure (double lastSinrValue) = 0;

};




/**
 * Template for the implementation of the NrUeCphySapProvider as a member
 * of an owner class of type C to which all methods are forwarded
 * 
 */
template <class C>
class MemberNrUeCphySapProvider : public NrUeCphySapProvider
{
public:
  MemberNrUeCphySapProvider (C* owner);

  // inherited from NrUeCphySapProvider
  virtual void Reset ();
  virtual void StartCellSearch (uint16_t dlEarfcn);
  virtual void SynchronizeWithEnb (uint16_t cellId);
  virtual void SynchronizeWithEnb (uint16_t cellId, uint16_t dlEarfcn);
  virtual void SetDlBandwidth (uint8_t dlBandwidth);
  virtual void ConfigureUplink (uint16_t ulEarfcn, uint8_t ulBandwidth);
  virtual void ConfigureReferenceSignalPower (int8_t referenceSignalPower);
  virtual void SetRnti (uint16_t rnti);
  virtual void SetTransmissionMode (uint8_t txMode);
  virtual void SetSrsConfigurationIndex (uint16_t srcCi);
  virtual void SetPa (double pa);

private:
  MemberNrUeCphySapProvider ();
  C* m_owner;
};

template <class C>
MemberNrUeCphySapProvider<C>::MemberNrUeCphySapProvider (C* owner)
  : m_owner (owner)
{
}

template <class C>
MemberNrUeCphySapProvider<C>::MemberNrUeCphySapProvider ()
{
}

template <class C>
void 
MemberNrUeCphySapProvider<C>::Reset ()
{
  m_owner->DoReset ();
}

template <class C>
void
MemberNrUeCphySapProvider<C>::StartCellSearch (uint16_t dlEarfcn)
{
  m_owner->DoStartCellSearch (dlEarfcn);
}

template <class C>
void
MemberNrUeCphySapProvider<C>::SynchronizeWithEnb (uint16_t cellId)
{
  m_owner->DoSynchronizeWithEnb (cellId);
}

template <class C>
void
MemberNrUeCphySapProvider<C>::SynchronizeWithEnb (uint16_t cellId, uint16_t dlEarfcn)
{
  m_owner->DoSynchronizeWithEnb (cellId, dlEarfcn);
}

template <class C>
void
MemberNrUeCphySapProvider<C>::SetDlBandwidth (uint8_t dlBandwidth)
{
  m_owner->DoSetDlBandwidth (dlBandwidth);
}

template <class C>
void 
MemberNrUeCphySapProvider<C>::ConfigureUplink (uint16_t ulEarfcn, uint8_t ulBandwidth)
{
  m_owner->DoConfigureUplink (ulEarfcn, ulBandwidth);
}

template <class C>
void 
MemberNrUeCphySapProvider<C>::ConfigureReferenceSignalPower (int8_t referenceSignalPower)
{
  m_owner->DoConfigureReferenceSignalPower (referenceSignalPower);
}

template <class C>
void
MemberNrUeCphySapProvider<C>::SetRnti (uint16_t rnti)
{
  m_owner->DoSetRnti (rnti);
}

template <class C>
void 
MemberNrUeCphySapProvider<C>::SetTransmissionMode (uint8_t txMode)
{
  m_owner->DoSetTransmissionMode (txMode);
}

template <class C>
void 
MemberNrUeCphySapProvider<C>::SetSrsConfigurationIndex (uint16_t srcCi)
{
  m_owner->DoSetSrsConfigurationIndex (srcCi);
}

template <class C>
void
MemberNrUeCphySapProvider<C>::SetPa (double pa)
{
  m_owner->DoSetPa (pa);
}


/**
 * Template for the implementation of the NrUeCphySapUser as a member
 * of an owner class of type C to which all methods are forwarded
 * 
 */
template <class C>
class MemberNrUeCphySapUser : public NrUeCphySapUser
{
public:
  MemberNrUeCphySapUser (C* owner);

  // methods inherited from NrUeCphySapUser go here
  virtual void RecvMasterInformationBlock (uint16_t cellId,
                                           NrRrcSap::MasterInformationBlock mib);
  virtual void RecvSystemInformationBlockType1 (uint16_t cellId,
                                                NrRrcSap::SystemInformationBlockType1 sib1);
  virtual void ReportUeMeasurements (NrUeCphySapUser::UeMeasurementsParameters params);

  virtual void NotifyRadioLinkFailure (double lastSinrValue);

private:
  MemberNrUeCphySapUser ();
  C* m_owner;
};

template <class C>
MemberNrUeCphySapUser<C>::MemberNrUeCphySapUser (C* owner)
  : m_owner (owner)
{
}

template <class C>
MemberNrUeCphySapUser<C>::MemberNrUeCphySapUser ()
{
}

template <class C> 
void 
MemberNrUeCphySapUser<C>::RecvMasterInformationBlock (uint16_t cellId,
                                                       NrRrcSap::MasterInformationBlock mib)
{
  m_owner->DoRecvMasterInformationBlock (cellId, mib);
}

template <class C>
void
MemberNrUeCphySapUser<C>::RecvSystemInformationBlockType1 (uint16_t cellId,
                                                            NrRrcSap::SystemInformationBlockType1 sib1)
{
  m_owner->DoRecvSystemInformationBlockType1 (cellId, sib1);
}

template <class C>
void
MemberNrUeCphySapUser<C>::ReportUeMeasurements (NrUeCphySapUser::UeMeasurementsParameters params)
{
  m_owner->DoReportUeMeasurements (params);
}

template <class C>
void
MemberNrUeCphySapUser<C>::NotifyRadioLinkFailure (double lastSinrValue)
{
  m_owner->DoNotifyRadioLinkFailure(lastSinrValue);
}



} // namespace ns3


#endif // NR_UE_CPHY_SAP_H

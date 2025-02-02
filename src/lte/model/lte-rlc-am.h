/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Manuel Requena <manuel.requena@cttc.es>
 *
 * Modified by Michele Polese <michele.polese@gmail.com> to add DC functionalities
 */

#ifndef LTE_RLC_AM_H
#define LTE_RLC_AM_H

#include <ns3/event-id.h>
#include <ns3/lte-rlc-sequence-number.h>
#include <ns3/lte-rlc.h>
#include <ns3/epc-x2-sap.h>
#include <ns3/lte-pdcp-header.h>

#include <vector>
#include <map>
#include <fstream>
#include <string>

#include "ns3/codel-queue-disc.h" 

namespace ns3 {

/**
 * LTE RLC Acknowledged Mode (AM), see 3GPP TS 36.322
 */
class LteRlcAm : public LteRlc
{
public:
  LteRlcAm ();
  virtual ~LteRlcAm ();
  static TypeId GetTypeId (void);
  virtual void DoDispose ();

  struct RetxPdu
  {
    Ptr<Packet> m_pdu;
    uint16_t    m_retxCount;
  };

  /**
   * RLC SAP
   */
  virtual void DoTransmitPdcpPdu (Ptr<Packet> p);

  /**
   * RLC EPC X2 SAP
   */
  virtual void DoSendMcPdcpSdu(EpcX2Sap::UeDataParams params);
  virtual void CalculatePathThroughput(std::ofstream *stream); //sjkang

  // LL HO
  std::vector < Ptr<Packet> > GetTxBuffer();
  uint32_t GetTxBufferSize();
  
  std::vector < RetxPdu > GetTxedBuffer();
  uint32_t GetTxedBufferSize();

  std::vector < RetxPdu > GetRetxBuffer();
  uint32_t GetRetxBufferSize();

  std::map < uint32_t, Ptr<Packet> > GetTransmittingRlcSduBuffer();
  uint32_t GetTransmittingRlcSduBufferSize();

  Ptr<Packet> GetSegmentedRlcsdu();
  ///< translate a vector of Rlc PDUs to Rlc SDUs 
  ///< and put the Rlc SDUs into m_transmittingRlcSdus.
  void  RlcPdusToRlcSdus (std::vector < RetxPdu >  Pdus);
  
  std::vector < Ptr<Packet> > GetTxedRlcSduBuffer (){
    return m_txedRlcSduBuffer;
  }

private:
  //whether the last SDU in the txonBuffer is a complete SDU.
  bool is_fragmented;

  //
  std::vector < Ptr <Packet> > m_txedRlcSduBuffer;
  uint32_t m_txedRlcSduBufferSize;
  
public:
  /**
   * MAC SAP
   */
  virtual void DoNotifyTxOpportunity (uint32_t bytes, uint8_t layer, uint8_t harqId);
  virtual void DoNotifyHarqDeliveryFailure ();
  virtual void DoNotifyDlHarqDeliveryFailure (uint8_t harqId);
  virtual void DoNotifyUlHarqDeliveryFailure (uint8_t harqId);
  virtual void DoReceivePdu (Ptr<Packet> p);
  void RecordingQueueStatistics() ; //sjkang1116
private:
  /**
   * This method will schedule a timeout at WaitReplyTimeout interval
   * in the future, unless a timer is already running for the cache,
   * in which case this method does nothing.
   */
  void ExpireReorderingTimer (void);
  void ExpirePollRetransmitTimer (void);
  void ExpireRbsTimer (void);

  /** 
   * method called when the T_status_prohibit timer expires
   * 
   */
  void ExpireStatusProhibitTimer (void);

  bool IsInsideReceivingWindow (SequenceNumber10 seqNumber);

  // LL HO
  bool IsInsideTransmittingWindow ();
  //Create RlcSduBuffer <seqNumber, RlcSDU> based on m_transmittingRlcSdus.
  //The buffer is ascending ordered on sequence number.
  void CreateRlcSduBuffer ();

// 
//   void ReassembleOutsideWindow (void);
//   void ReassembleSnLessThan (uint16_t seqNumber);
// 
  void ReassembleAndDeliver (Ptr<Packet> packet);
  void TriggerReceivePdcpPdu(Ptr<Packet> p);

  void Reassemble (Ptr<Packet> Packet);

  void DoReportBufferStatus ();

  std::string GetBufferSizeFilename();
  void SetBufferSizeFilename(std::string filename);
  void BufferSizeTrace();
  void SendAssistantInformation(EpcX2Sap::AssistantInformationForSplitting info); //sjkang1114
  void SendLteAssistantInformation(EpcX2Sap::AssistantInformationForSplitting info); //sjkang1114
  void DoRequestAssistantInfo();
private:
    std::vector < Ptr<Packet> > m_txonBuffer;       // Transmission buffer

    struct RetxSegPdu
    {
      Ptr<Packet> m_pdu;
      uint16_t    m_retxCount;
      bool      m_lastSegSent;    // all segments sent, waiting for ACK
    };

  // LL HO: store a complete version of the incomplete RLC SDU at the 
  // edge of the m_txonBuffer during the segmentation process.
  // This SDU will be forwarded to target eNB in lossless HO
  // to assure no packet is lost.
  Ptr<Packet> m_segmented_rlcsdu;

  std::vector <RetxPdu> m_txedBuffer;  ///< Buffer for transmitted and retransmitted PDUs 
                                       ///< that have not been acked but are not considered 
                                       ///< for retransmission 
  std::vector <RetxPdu> m_retxBuffer;  ///< Buffer for PDUs considered for retransmission
  std::vector <RetxSegPdu> m_retxSegBuffer;  // buffer for AM PDU segments

  Ptr<CoDelQueueDisc> m_txonQueue;

  ///< LL HO: stores RLC SDUs that is not acked 
  ///< and forwarded to target eNB during lossless handover.
  std::vector < Ptr<Packet> > m_transmittingRlcSdus;
  uint32_t m_transmittingRlcSduBufferSize;
  std::map <uint32_t, Ptr <Packet> > m_transmittingRlcSduBuffer;

    uint32_t m_txonBufferSize;
    uint32_t m_retxBufferSize;
    uint32_t m_txedBufferSize;

    bool     m_statusPduRequested;
    uint32_t m_statusPduBufferSize;

    struct PduBuffer
    {
      SequenceNumber10  m_seqNumber;
      std::list < Ptr<Packet> >  m_byteSegments;

      bool      m_pduComplete;
      uint16_t  m_totalSize;
      uint16_t  m_currSize;
    };

    std::map <uint16_t, PduBuffer > m_rxonBuffer; // Reception buffer

    Ptr<Packet> m_controlPduBuffer;               // Control PDU buffer (just one PDU)

    // SDU reassembly
//   std::vector < Ptr<Packet> > m_reasBuffer;     // Reassembling buffer
// 
    std::list < Ptr<Packet> > m_sdusBuffer;       // List of SDUs in a packet (PDU)
    std::list < Ptr<Packet> > m_sdusAssembleBuffer;

  /**
   * State variables. See section 7.1 in TS 36.322
   */
  // Transmitting side
  SequenceNumber10 m_vtA;                   // VT(A)
  SequenceNumber10 m_vtMs;                  // VT(MS)
  SequenceNumber10 m_vtS;                   // VT(S)
  SequenceNumber10 m_pollSn;                // POLL_SN

  // Receiving side
  SequenceNumber10 m_vrR;                   // VR(R)
  SequenceNumber10 m_vrMr;                  // VR(MR)
  SequenceNumber10 m_vrX;                   // VR(X)
  SequenceNumber10 m_vrMs;                  // VR(MS)
  SequenceNumber10 m_vrH;                   // VR(H)

  /**
   * Counters. See section 7.1 in TS 36.322
   */
  uint32_t m_pduWithoutPoll;
  uint32_t m_byteWithoutPoll;

  /**
   * Constants. See section 7.2 in TS 36.322
   */
  uint16_t m_windowSize;

  /**
   * Timers. See section 7.3 in TS 36.322
   */
  EventId m_pollRetransmitTimer;
  Time    m_pollRetransmitTimerValue;
  EventId m_reorderingTimer;
  Time    m_reorderingTimerValue;
  EventId m_statusProhibitTimer;
  Time    m_statusProhibitTimerValue;
  EventId m_rbsTimer;
  Time    m_rbsTimerValue;

  /**
   * Configurable parameters. See section 7.4 in TS 36.322
   */
  uint16_t m_maxRetxThreshold;  /// \todo How these parameters are configured???
  uint16_t m_pollPdu;
  uint16_t m_pollByte;
  
  bool m_txOpportunityForRetxAlwaysBigEnough;
  bool m_pollRetransmitTimerJustExpired;

  /**
   * SDU Reassembling state
   */
  typedef enum { NONE            = 0,
                 WAITING_S0_FULL = 1,
                 WAITING_SI_SF   = 2 } ReassemblingState_t;
  ReassemblingState_t m_reassemblingState;
  ReassemblingState_t m_assemblingState; //state of the RlcPduToRlcSdu assembling used for handover.
  Ptr<Packet> m_keepS0;
  Ptr<Packet> m_keepS0Reassemble;

  /**
   * Expected Sequence Number
   */
  SequenceNumber10 m_expectedSeqNumber;

  SequenceNumber10 m_reassembleExpectedSeqNumber;

  std::map <uint8_t, uint16_t> m_harqIdToSnMap;

  uint32_t m_maxTxBufferSize;

  std::string m_bufferSizeFilename;
  std::ofstream m_bufferSizeFile;
  EventId m_traceBufferSizeEvent;

  bool m_enableAqm;
  uint64_t TotalPackets=0;
  uint64_t lastSumPacketSize;
  uint64_t sumPacketSize=0;
  double TotalTime=0.0;

  uint16_t reporting_Start=0; //sjkang
  uint16_t TxOn_QueingDelay; //sjkang
  uint16_t ReTx_QueingDelay; //sjkang


};

} // namespace ns3

#endif // LTE_RLC_AM_H

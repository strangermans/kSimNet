/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2017, NYU WIRELESS, Tandon School of Engineering, New York University
 * Copyright (c) 2017, University of Padova, Dep. of Information Engineering, SIGNET lab
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
 * Modified by:  Russell Ford
 *                  Low lat 
 *               Michele Polese <michele.polese@gmail.com>
 *                  Dual Connectivity functionalities
 */

#include "nr-rlc-um-lowlat.h"

#include "ns3/simulator.h"
#include "ns3/log.h"

#include "ns3/nr-rlc-header.h"
#include "ns3/nr-rlc-sdu-status-tag.h"
#include "ns3/nr-rlc-tag.h"
#include <fstream>
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NrRlcUmLowLat");

NS_OBJECT_ENSURE_REGISTERED (NrRlcUmLowLat);

uint32_t NrRlcUmLowLat::m_numArrivalsToAvg = 20;

NrRlcUmLowLat::NrRlcUmLowLat ()
  : m_maxTxBufferSize (10 * 1024),
    m_txBufferSize (0),
    m_sequenceNumber (0),
    m_vrUr (0),
    m_vrUx (0),
    m_vrUh (0),
    m_windowSize (512),
    m_expectedSeqNumber (0),
		m_currTotalPacketSize (0),
		//m_lastArrivalTime (0),
		m_arrivalRate (0.0),
    m_bsrReported(false)
		//m_forgetFactor (0.1)
{
  NS_LOG_FUNCTION (this);
  m_reassemblingState = WAITING_S0_FULL;
  m_ngcX2RlcUser = new NgcX2RlcSpecificUser<NrRlcUmLowLat> (this);
  m_ngcX2RlcProvider = 0;
  TxOn_QueingDelay = 0;
 // m_drbId=0;//sjkang

}

NrRlcUmLowLat::~NrRlcUmLowLat ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
NrRlcUmLowLat::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrRlcUmLowLat")
    .SetParent<NrRlc> ()
    .SetGroupName("Nr")
    .AddConstructor<NrRlcUmLowLat> ()
    .AddAttribute ("MaxTxBufferSize",
                   "Maximum Size of the Transmission Buffer (in Bytes)",
                   UintegerValue (10 * 1024),
                   MakeUintegerAccessor (&NrRlcUmLowLat::m_maxTxBufferSize),
                   MakeUintegerChecker<uint32_t> ())
	 .AddAttribute ("ReportBufferStatusTimer",
									"How much to wait to issue a new Report Buffer Status since the last time "
									"a new SDU was received",
									TimeValue (MilliSeconds (20)),
									MakeTimeAccessor (&NrRlcUmLowLat::m_rbsTimerValue),
									MakeTimeChecker ())
	 .AddAttribute ("ReorderingTimeExpires",
									"Time to wait for out of order PDUs"
									"a new SDU was received",
									TimeValue (MilliSeconds (100.0)),
									MakeTimeAccessor (&NrRlcUmLowLat::m_reorderingTimeExpires),
									MakeTimeChecker ())
    ;
  return tid;
}

void
NrRlcUmLowLat::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_reorderingTimer.Cancel ();
  m_rbsTimer.Cancel ();
  m_sendAssistatInfo.Cancel();
  m_reorderingQueueStatistic.Cancel();
  measuringQusizeQueueDelayStream->close();
  //delete ( m_ngcX2RlcProvider);
  //delete (m_ngcX2RlcUser);
  TxOn_QueingDelay =0;
  m_txBufferSize  = 0;
  NrRlc::DoDispose ();
}

/**
 * RLC SAP
 */

void
NrRlcUmLowLat::DoTransmitPdcpPdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());

  if (m_txBufferSize + p->GetSize () <= m_maxTxBufferSize)
    {
      /** Store arrival time */
      RlcTag timeTag (Simulator::Now ());
      p->AddPacketTag (timeTag);


      if (m_ngcX2RlcProvider !=0 && reporting_start==0){ //sjkang1114
       NgcX2Sap::AssistantInformationForSplitting info;
      std::cout << "will send assistant info from NrRlcUmLowRat" <<std::endl;
      RecordingQueueStatistics(); //sjkang1116
      Simulator::Schedule(MilliSeconds(10),&NrRlcUmLowLat::DoSendAssistantInformation,this, info);
      reporting_start=1;
       }

      NrRlcSduStatusTag tag;
      tag.SetStatus (NrRlcSduStatusTag::FULL_SDU);
      p->AddPacketTag (tag);

      NS_LOG_LOGIC ("Tx Buffer: New packet added");
      m_txBuffer.push_back (p);
      m_txBufferSize += p->GetSize ();
      NS_LOG_LOGIC ("NumOfBuffers = " << m_txBuffer.size() );
      NS_LOG_LOGIC ("txBufferSize = " << m_txBufferSize);

      if (m_recentArrivalTimes.size () == m_numArrivalsToAvg)
      {
      	m_recentArrivalTimes.pop_front ();
      	m_currTotalPacketSize -= m_recentPacketSizes.front ();
      	m_recentPacketSizes.pop_front ();
      }
      m_recentArrivalTimes.push_back ((uint32_t)timeTag.GetSenderTimestamp ().GetMicroSeconds ());
      m_recentPacketSizes.push_back (p->GetSize ());
      m_currTotalPacketSize += p->GetSize ();
      double timeDiff = (m_recentArrivalTimes.back () - m_recentArrivalTimes.front ()) * 1e-6;
      //m_arrivalRate = (1 - m_forgetFactor) * (p->GetSize () / timeDiff) + m_forgetFactor * m_arrivalRate;
      m_arrivalRate = m_currTotalPacketSize / timeDiff;
    }
  else
    {
      // Discard full RLC SDU
      NS_LOG_LOGIC ("TxBuffer is full. RLC SDU discarded");
      NS_LOG_LOGIC ("MaxTxBufferSize = " << m_maxTxBufferSize);
      NS_LOG_LOGIC ("txBufferSize    = " << m_txBufferSize);
      NS_LOG_LOGIC ("packet size     = " << p->GetSize ());
    }

  /** Report Buffer Status */

  DoReportBufferStatus ();
  m_bsrReported = true;
  m_rbsTimer.Cancel ();
  m_rbsTimer = Simulator::Schedule (m_rbsTimerValue, &NrRlcUmLowLat::ExpireRbsTimer, this);
}

void 
NrRlcUmLowLat::DoSendMcPdcpSdu(NgcX2Sap::UeDataParams params)
{
  NS_LOG_FUNCTION(this);
  DoTransmitPdcpPdu(params.ueData);
}

/**
 * MAC SAP
 */
void
NrRlcUmLowLat::SetRlcSN(){
	m_sequenceNumber =0;
}

void
NrRlcUmLowLat::DoNotifyTxOpportunity (uint32_t bytes, uint8_t layer, uint8_t harqId)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << bytes);

  if (bytes <= 2)
    {
      // Stingy MAC: Header fix part is 2 bytes, we need more bytes for the data
      NS_LOG_LOGIC ("TX opportunity too small = " << bytes);
      return;
    }

  if (bytes > m_txBufferSize)
   {
     NS_LOG_DEBUG("NrRlcUmLowLat rnti " << m_rnti << " lcid " << m_lcid << " allocated " << bytes << " bufsize " << m_txBufferSize);
   }

  Ptr<Packet> packet = Create<Packet> ();
  NrRlcHeader rlcHeader;

  // Build Data field
  uint32_t nextSegmentSize = bytes - 2;
  uint32_t nextSegmentId = 1;
  uint32_t dataFieldTotalSize = 0;
  uint32_t dataFieldAddedSize = 0;
  std::vector < Ptr<Packet> > dataField;

  // Remove the first packet from the transmission buffer.
  // If only a segment of the packet is taken, then the remaining is given back later
  if ( m_txBuffer.size () == 0 )
    {
      NS_LOG_LOGIC ("No data pending");
      return;
    }

  NS_LOG_LOGIC ("SDUs in TxBuffer  = " << m_txBuffer.size ());
  NS_LOG_LOGIC ("First SDU buffer  = " << *(m_txBuffer.begin()));
  NS_LOG_LOGIC ("First SDU size    = " << (*(m_txBuffer.begin()))->GetSize ());
  NS_LOG_LOGIC ("Next segment size = " << nextSegmentSize);
  NS_LOG_LOGIC ("Remove SDU from TxBuffer");
  Ptr<Packet> firstSegment = (*(m_txBuffer.begin ()))->Copy ();
  m_txBufferSize -= (*(m_txBuffer.begin()))->GetSize ();
  NS_LOG_LOGIC ("txBufferSize      = " << m_txBufferSize );
  m_txBuffer.erase (m_txBuffer.begin ());

  while ( firstSegment && (firstSegment->GetSize () > 0) && (nextSegmentSize > 0) )
    {
      NS_LOG_LOGIC ("WHILE ( firstSegment && firstSegment->GetSize > 0 && nextSegmentSize > 0 )");
      NS_LOG_LOGIC ("    firstSegment size = " << firstSegment->GetSize ());
      NS_LOG_LOGIC ("    nextSegmentSize   = " << nextSegmentSize);
      if ( (firstSegment->GetSize () > nextSegmentSize) ||
           // Segment larger than 2047 octets can only be mapped to the end of the Data field
           (firstSegment->GetSize () > 2047)
         )
        {
          // Take the minimum size, due to the 2047-bytes 3GPP exception
          // This exception is due to the length of the LI field (just 11 bits)
          uint32_t currSegmentSize = std::min (firstSegment->GetSize (), nextSegmentSize);

          NS_LOG_LOGIC ("    IF ( firstSegment > nextSegmentSize ||");
          NS_LOG_LOGIC ("         firstSegment > 2047 )");

          // Segment txBuffer.FirstBuffer and
          // Give back the remaining segment to the transmission buffer
          Ptr<Packet> newSegment = firstSegment->CreateFragment (0, currSegmentSize);
          NS_LOG_LOGIC ("    newSegment size   = " << newSegment->GetSize ());

          // Status tag of the new and remaining segments
          // Note: This is the only place where a PDU is segmented and
          // therefore its status can change
          NrRlcSduStatusTag oldTag, newTag;
          firstSegment->RemovePacketTag (oldTag);
          newSegment->RemovePacketTag (newTag);
          if (oldTag.GetStatus () == NrRlcSduStatusTag::FULL_SDU)
            {
              newTag.SetStatus (NrRlcSduStatusTag::FIRST_SEGMENT);
              oldTag.SetStatus (NrRlcSduStatusTag::LAST_SEGMENT);
            }
          else if (oldTag.GetStatus () == NrRlcSduStatusTag::LAST_SEGMENT)
            {
              newTag.SetStatus (NrRlcSduStatusTag::MIDDLE_SEGMENT);
              //oldTag.SetStatus (NrRlcSduStatusTag::LAST_SEGMENT);
            }

          // Give back the remaining segment to the transmission buffer
          firstSegment->RemoveAtStart (currSegmentSize);
          NS_LOG_LOGIC ("    firstSegment size (after RemoveAtStart) = " << firstSegment->GetSize ());
          if (firstSegment->GetSize () > 0)
            {
              firstSegment->AddPacketTag (oldTag);

              m_txBuffer.insert (m_txBuffer.begin (), firstSegment);
              m_txBufferSize += (*(m_txBuffer.begin()))->GetSize ();

              NS_LOG_LOGIC ("    TX buffer: Give back the remaining segment");
              NS_LOG_LOGIC ("    TX buffers = " << m_txBuffer.size ());
              NS_LOG_LOGIC ("    Front buffer size = " << (*(m_txBuffer.begin()))->GetSize ());
              NS_LOG_LOGIC ("    txBufferSize = " << m_txBufferSize );
            }
          else
            {
              // Whole segment was taken, so adjust tag
              if (newTag.GetStatus () == NrRlcSduStatusTag::FIRST_SEGMENT)
                {
                  newTag.SetStatus (NrRlcSduStatusTag::FULL_SDU);
                }
              else if (newTag.GetStatus () == NrRlcSduStatusTag::MIDDLE_SEGMENT)
                {
                  newTag.SetStatus (NrRlcSduStatusTag::LAST_SEGMENT);
                }
            }
          // Segment is completely taken or
          // the remaining segment is given back to the transmission buffer
          firstSegment = 0;

          // Put status tag once it has been adjusted
          newSegment->AddPacketTag (newTag);

          // Add Segment to Data field
          dataFieldAddedSize = newSegment->GetSize ();
          dataFieldTotalSize += dataFieldAddedSize;
          dataField.push_back (newSegment);
          newSegment = 0;

          // ExtensionBit (Next_Segment - 1) = 0
          rlcHeader.PushExtensionBit (NrRlcHeader::DATA_FIELD_FOLLOWS);

          // no LengthIndicator for the last one

          nextSegmentSize -= dataFieldAddedSize;
          nextSegmentId++;

          // nextSegmentSize MUST be zero (only if segment is smaller or equal to 2047)

          // (NO more segments) → exit
          // break;
        }
      else if ( (nextSegmentSize - firstSegment->GetSize () <= 2) || (m_txBuffer.size () == 0) )
        {
          NS_LOG_LOGIC ("    IF nextSegmentSize - firstSegment->GetSize () <= 2 || txBuffer.size == 0");
          // Add txBuffer.FirstBuffer to DataField
          dataFieldAddedSize = firstSegment->GetSize ();
          dataFieldTotalSize += dataFieldAddedSize;
          dataField.push_back (firstSegment);
          firstSegment = 0;

          // ExtensionBit (Next_Segment - 1) = 0
          rlcHeader.PushExtensionBit (NrRlcHeader::DATA_FIELD_FOLLOWS);

          // no LengthIndicator for the last one

          nextSegmentSize -= dataFieldAddedSize;
          nextSegmentId++;

          NS_LOG_LOGIC ("        SDUs in TxBuffer  = " << m_txBuffer.size ());
          if (m_txBuffer.size () > 0)
            {
              NS_LOG_LOGIC ("        First SDU buffer  = " << *(m_txBuffer.begin()));
              NS_LOG_LOGIC ("        First SDU size    = " << (*(m_txBuffer.begin()))->GetSize ());
            }
          NS_LOG_LOGIC ("        Next segment size = " << nextSegmentSize);

          // nextSegmentSize <= 2 (only if txBuffer is not empty)

          // (NO more segments) → exit
          // break;
        }
      else // (firstSegment->GetSize () < m_nextSegmentSize) && (m_txBuffer.size () > 0)
        {
          NS_LOG_LOGIC ("    IF firstSegment < NextSegmentSize && txBuffer.size > 0");
          // Add txBuffer.FirstBuffer to DataField
          dataFieldAddedSize = firstSegment->GetSize ();
          dataFieldTotalSize += dataFieldAddedSize;
          dataField.push_back (firstSegment);

          // ExtensionBit (Next_Segment - 1) = 1
          rlcHeader.PushExtensionBit (NrRlcHeader::E_LI_FIELDS_FOLLOWS);

          // LengthIndicator (Next_Segment)  = txBuffer.FirstBuffer.length()
          rlcHeader.PushLengthIndicator (firstSegment->GetSize ());

          nextSegmentSize -= ((nextSegmentId % 2) ? (2) : (1)) + dataFieldAddedSize;
          nextSegmentId++;

          NS_LOG_LOGIC ("        SDUs in TxBuffer  = " << m_txBuffer.size ());
          if (m_txBuffer.size () > 0)
            {
              NS_LOG_LOGIC ("        First SDU buffer  = " << *(m_txBuffer.begin()));
              NS_LOG_LOGIC ("        First SDU size    = " << (*(m_txBuffer.begin()))->GetSize ());
            }
          NS_LOG_LOGIC ("        Next segment size = " << nextSegmentSize);
          NS_LOG_LOGIC ("        Remove SDU from TxBuffer");

          // (more segments)
          firstSegment = (*(m_txBuffer.begin ()))->Copy ();
          m_txBufferSize -= (*(m_txBuffer.begin()))->GetSize ();
          m_txBuffer.erase (m_txBuffer.begin ());
          NS_LOG_LOGIC ("        txBufferSize = " << m_txBufferSize );
        }

    }

  // Build RLC header
  rlcHeader.SetSequenceNumber (m_sequenceNumber++);

  // Build RLC PDU with DataField and Header
  std::vector< Ptr<Packet> >::iterator it;
  it = dataField.begin ();

  uint8_t framingInfo = 0;

  // FIRST SEGMENT
  NrRlcSduStatusTag tag;
  (*it)->RemovePacketTag (tag);
  if ( (tag.GetStatus () == NrRlcSduStatusTag::FULL_SDU) ||
        (tag.GetStatus () == NrRlcSduStatusTag::FIRST_SEGMENT) )
    {
      framingInfo |= NrRlcHeader::FIRST_BYTE;
    }
  else
    {
      framingInfo |= NrRlcHeader::NO_FIRST_BYTE;
    }
  (*it)->AddPacketTag (tag);

  while (it < dataField.end ())
    {
      NS_LOG_LOGIC ("Adding SDU/segment to packet, length = " << (*it)->GetSize ());

      packet->AddAtEnd (*it);
      it++;
    }

  // LAST SEGMENT (Note: There could be only one and be the first one)
  it--;
  (*it)->RemovePacketTag (tag);
  if ( (tag.GetStatus () == NrRlcSduStatusTag::FULL_SDU) ||
        (tag.GetStatus () == NrRlcSduStatusTag::LAST_SEGMENT) )
    {
      framingInfo |= NrRlcHeader::LAST_BYTE;
    }
  else
    {
      framingInfo |= NrRlcHeader::NO_LAST_BYTE;
    }
  (*it)->AddPacketTag (tag);

  rlcHeader.SetFramingInfo (framingInfo);

  NS_LOG_LOGIC ("RLC header: " << rlcHeader);
  packet->AddHeader (rlcHeader);

  // Sender timestamp
  RlcTag rlcTag (Simulator::Now ());
  packet->AddByteTag (rlcTag);
  m_txPdu (m_rnti, m_lcid, packet->GetSize ());

  // Send RLC PDU to MAC layer
  NrMacSapProvider::TransmitPduParameters params;
  params.pdu = packet;
  params.rnti = m_rnti;
  params.lcid = m_lcid;
  params.layer = layer;
  params.harqProcessId = harqId;
//std::cout <<"TX   " << this << "\t" << Simulator::Now().GetSeconds()<<"\t"<<m_sequenceNumber << "\t"<< packet->GetSize()<<std::endl;
  m_macSapProvider->TransmitPdu (params);

  if (! m_txBuffer.empty ())
    {
      m_rbsTimer.Cancel ();
      m_rbsTimer = Simulator::Schedule (m_rbsTimerValue, &NrRlcUmLowLat::ExpireRbsTimer, this);
    }

  m_bsrReported = false;

  DoReportBufferStatus ();
}

void
NrRlcUmLowLat::DoNotifyHarqDeliveryFailure ()
{
  NS_LOG_FUNCTION (this);
}

std::vector < Ptr<Packet> > 
NrRlcUmLowLat::GetTxBuffer()
{
  return m_txBuffer;
}
void
NrRlcUmLowLat::ClearTxBuffer()
{
	m_txBuffer.clear();
}
void
NrRlcUmLowLat::DoReceivePdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());

  // Receiver timestamp
  RlcTag rlcTag;
  Time delay;
  if (p->FindFirstMatchingByteTag (rlcTag))
    {
      delay = Simulator::Now() - rlcTag.GetSenderTimestamp ();
    }
  m_rxPdu (m_rnti, m_lcid, p->GetSize (), delay.GetNanoSeconds ());

  // 5.1.2.2 Receive operations

  // Get RLC header parameters
  NrRlcHeader rlcHeader;
  p->PeekHeader (rlcHeader);
  NS_LOG_LOGIC ("RLC header: " << rlcHeader);
  SequenceNumber10 seqNumber = rlcHeader.GetSequenceNumber ();

  // 5.1.2.2.1 General
  // The receiving UM RLC entity shall maintain a reordering window according to state variable VR(UH) as follows:
  // - a SN falls within the reordering window if (VR(UH) - UM_Window_Size) <= SN < VR(UH);
  // - a SN falls outside of the reordering window otherwise.
  // When receiving an UMD PDU from lower layer, the receiving UM RLC entity shall:
  // - either discard the received UMD PDU or place it in the reception buffer (see sub clause 5.1.2.2.2);
  // - if the received UMD PDU was placed in the reception buffer:
  // - update state variables, reassemble and deliver RLC SDUs to upper layer and start/stop t-Reordering as needed (see sub clause 5.1.2.2.3);
  // When t-Reordering expires, the receiving UM RLC entity shall:
  // - update state variables, reassemble and deliver RLC SDUs to upper layer and start t-Reordering as needed (see sub clause 5.1.2.2.4).

  // 5.1.2.2.2 Actions when an UMD PDU is received from lower layer
  // When an UMD PDU with SN = x is received from lower layer, the receiving UM RLC entity shall:
  // - if VR(UR) < x < VR(UH) and the UMD PDU with SN = x has been received before; or
  // - if (VR(UH) - UM_Window_Size) <= x < VR(UR):
  //    - discard the received UMD PDU;
  // - else:
  //    - place the received UMD PDU in the reception buffer.

  NS_LOG_LOGIC ("VR(UR) = " << m_vrUr);
  NS_LOG_LOGIC ("VR(UX) = " << m_vrUx);
  NS_LOG_LOGIC ("VR(UH) = " << m_vrUh);
  NS_LOG_LOGIC ("SN = " << seqNumber);

  m_vrUr.SetModulusBase (m_vrUh - m_windowSize);
  m_vrUh.SetModulusBase (m_vrUh - m_windowSize);
  seqNumber.SetModulusBase (m_vrUh - m_windowSize);

  if ( ( (m_vrUr < seqNumber) && (seqNumber < m_vrUh) && (m_rxBuffer.count (seqNumber.GetValue ()) > 0) ) ||
       ( ((m_vrUh - m_windowSize) <= seqNumber) && (seqNumber < m_vrUr) )
     )
    {
      NS_LOG_LOGIC ("PDU discarded");
      p = 0;
      return;
    }
  else
    {
      NS_LOG_LOGIC ("Place PDU in the reception buffer");
      m_rxBuffer[seqNumber.GetValue ()] = p;
    }


  // 5.1.2.2.3 Actions when an UMD PDU is placed in the reception buffer
  // When an UMD PDU with SN = x is placed in the reception buffer, the receiving UM RLC entity shall:

  // - if x falls outside of the reordering window:
  //    - update VR(UH) to x + 1;
  //    - reassemble RLC SDUs from any UMD PDUs with SN that falls outside of the reordering window, remove
  //      RLC headers when doing so and deliver the reassembled RLC SDUs to upper layer in ascending order of the
  //      RLC SN if not delivered before;
  //    - if VR(UR) falls outside of the reordering window:
  //        - set VR(UR) to (VR(UH) - UM_Window_Size);

  if ( ! IsInsideReorderingWindow (seqNumber))
    {
      NS_LOG_LOGIC ("SN is outside the reordering window");

      m_vrUh = seqNumber + 1;
      NS_LOG_LOGIC ("New VR(UH) = " << m_vrUh);

      ReassembleOutsideWindow ();

      if ( ! IsInsideReorderingWindow (m_vrUr) )
        {
          m_vrUr = m_vrUh - m_windowSize;
          NS_LOG_LOGIC ("VR(UR) is outside the reordering window");
          NS_LOG_LOGIC ("New VR(UR) = " << m_vrUr);
        }
    }

  // - if the reception buffer contains an UMD PDU with SN = VR(UR):
  //    - update VR(UR) to the SN of the first UMD PDU with SN > current VR(UR) that has not been received;
  //    - reassemble RLC SDUs from any UMD PDUs with SN < updated VR(UR), remove RLC headers when doing
  //      so and deliver the reassembled RLC SDUs to upper layer in ascending order of the RLC SN if not delivered
  //      before;

  if ( m_rxBuffer.count (m_vrUr.GetValue ()) > 0 )
    {
      NS_LOG_LOGIC ("Reception buffer contains SN = " << m_vrUr);

      std::map <uint16_t, Ptr<Packet> >::iterator it;
      uint16_t newVrUr;
      SequenceNumber10 oldVrUr = m_vrUr;

      it = m_rxBuffer.find (m_vrUr.GetValue ());
      newVrUr = (it->first) + 1;
      while ( m_rxBuffer.count (newVrUr) > 0 )
        {
          newVrUr++;
        }
      m_vrUr = newVrUr;
      NS_LOG_LOGIC ("New VR(UR) = " << m_vrUr);

      ReassembleSnInterval (oldVrUr, m_vrUr);
    }

  // m_vrUh can change previously, set new modulus base
  // for the t-Reordering timer-related comparisons
  m_vrUr.SetModulusBase (m_vrUh - m_windowSize);
  m_vrUx.SetModulusBase (m_vrUh - m_windowSize);
  m_vrUh.SetModulusBase (m_vrUh - m_windowSize);

  // - if t-Reordering is running:
  //    - if VR(UX) <= VR(UR); or
  //    - if VR(UX) falls outside of the reordering window and VR(UX) is not equal to VR(UH)::
  //        - stop and reset t-Reordering;
  if ( m_reorderingTimer.IsRunning () )
    {
      NS_LOG_LOGIC ("Reordering timer is running");

      if ( (m_vrUx <= m_vrUr) ||
           ((! IsInsideReorderingWindow (m_vrUx)) && (m_vrUx != m_vrUh)) )
        {
          NS_LOG_LOGIC ("Stop reordering timer");
          m_reorderingTimer.Cancel ();
        }
    }

  // - if t-Reordering is not running (includes the case when t-Reordering is stopped due to actions above):
  //    - if VR(UH) > VR(UR):
  //        - start t-Reordering;
  //        - set VR(UX) to VR(UH).
  if ( ! m_reorderingTimer.IsRunning () )
    {
      NS_LOG_LOGIC ("Reordering timer is not running");

      if ( m_vrUh > m_vrUr )
        {
          NS_LOG_LOGIC ("VR(UH) > VR(UR)");
          NS_LOG_LOGIC ("Start reordering timer");
          m_reorderingTimer = Simulator::Schedule (m_reorderingTimeExpires,
                                                   &NrRlcUmLowLat::ExpireReorderingTimer ,this);
          m_vrUx = m_vrUh;
          NS_LOG_LOGIC ("New VR(UX) = " << m_vrUx);
        }
    }

}


bool
NrRlcUmLowLat::IsInsideReorderingWindow (SequenceNumber10 seqNumber)
{
  NS_LOG_FUNCTION (this << seqNumber);
  NS_LOG_LOGIC ("Reordering Window: " <<
                m_vrUh << " - " << m_windowSize << " <= " << seqNumber << " < " << m_vrUh);

  m_vrUh.SetModulusBase (m_vrUh - m_windowSize);
  seqNumber.SetModulusBase (m_vrUh - m_windowSize);

  if ( ((m_vrUh - m_windowSize) <= seqNumber) && (seqNumber < m_vrUh))
    {
      NS_LOG_LOGIC (seqNumber << " is INSIDE the reordering window");
      return true;
    }
  else
    {
      NS_LOG_LOGIC (seqNumber << " is OUTSIDE the reordering window");
      return false;
    }
}


void
NrRlcUmLowLat::ReassembleAndDeliver (Ptr<Packet> packet)
{
  NrRlcHeader rlcHeader;
  packet->RemoveHeader (rlcHeader);
  uint8_t framingInfo = rlcHeader.GetFramingInfo ();
  SequenceNumber10 currSeqNumber = rlcHeader.GetSequenceNumber ();
  bool expectedSnLost;

  if ( currSeqNumber != m_expectedSeqNumber )
    {
      expectedSnLost = true;
      NS_LOG_LOGIC ("There are losses. Expected SN = " << m_expectedSeqNumber << ". Current SN = " << currSeqNumber);
      m_expectedSeqNumber = currSeqNumber + 1;
    }
  else
    {
      expectedSnLost = false;
      NS_LOG_LOGIC ("No losses. Expected SN = " << m_expectedSeqNumber << ". Current SN = " << currSeqNumber);
      m_expectedSeqNumber++;
    }

  // Build list of SDUs
  uint8_t extensionBit;
  uint16_t lengthIndicator;
  do
    {
      extensionBit = rlcHeader.PopExtensionBit ();
      NS_LOG_LOGIC ("E = " << (uint16_t)extensionBit);

      if ( extensionBit == 0 )
        {
          m_sdusBuffer.push_back (packet);
        }
      else // extensionBit == 1
        {
          lengthIndicator = rlcHeader.PopLengthIndicator ();
          NS_LOG_LOGIC ("LI = " << lengthIndicator);

          // Check if there is enough data in the packet
          if ( lengthIndicator >= packet->GetSize () )
            {
              NS_LOG_LOGIC ("INTERNAL ERROR: Not enough data in the packet (" << packet->GetSize () << "). Needed LI=" << lengthIndicator);
            }

          // Split packet in two fragments
          Ptr<Packet> data_field = packet->CreateFragment (0, lengthIndicator);
          packet->RemoveAtStart (lengthIndicator);

          m_sdusBuffer.push_back (data_field);
        }
    }
  while ( extensionBit == 1 );

  std::list < Ptr<Packet> >::iterator it;

  // Current reassembling state
  if      (m_reassemblingState == WAITING_S0_FULL)  NS_LOG_LOGIC ("Reassembling State = 'WAITING_S0_FULL'");
  else if (m_reassemblingState == WAITING_SI_SF)    NS_LOG_LOGIC ("Reassembling State = 'WAITING_SI_SF'");
  else                                              NS_LOG_LOGIC ("Reassembling State = Unknown state");

  // Received framing Info
  NS_LOG_LOGIC ("Framing Info = " << (uint16_t)framingInfo);

  // Reassemble the list of SDUs (when there is no losses)
  if (!expectedSnLost)
    {
      switch (m_reassemblingState)
        {
          case WAITING_S0_FULL:
                  switch (framingInfo)
                    {
                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                              * Deliver one or multiple PDUs
                              */
                              for ( it = m_sdusBuffer.begin () ; it != m_sdusBuffer.end () ; it++ )
                                {
                                  TriggerReceivePdcpPdu (*it);
                                }
                              m_sdusBuffer.clear ();
                      break;

                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                              * Deliver full PDUs
                              */
                              while ( m_sdusBuffer.size () > 1 )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }

                              /**
                              * Keep S0
                              */
                              m_keepS0 = m_sdusBuffer.front ();
                              m_sdusBuffer.pop_front ();
                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_reassemblingState = WAITING_S0_FULL;
                                }
                              else
                                {
                                  m_reassemblingState = WAITING_SI_SF;
                                }

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              if ( m_sdusBuffer.size () > 0 )
                                {
                                  /**
                                   * Deliver zero, one or multiple PDUs
                                   */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                   * Keep S0
                                   */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      default:
                              /**
                              * ERROR: Transition not possible
                              */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          case WAITING_SI_SF:
                  switch (framingInfo)
                    {
                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                              * Deliver (Kept)S0 + SN
                              */
                              m_keepS0->AddAtEnd (m_sdusBuffer.front ());
                              m_sdusBuffer.pop_front ();
                              TriggerReceivePdcpPdu (m_keepS0);

                              /**
                                * Deliver zero, one or multiple PDUs
                                */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                              * Keep SI
                              */
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_keepS0->AddAtEnd (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                              else // m_sdusBuffer.size () > 1
                                {
                                  /**
                                  * Deliver (Kept)S0 + SN
                                  */
                                  m_keepS0->AddAtEnd (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                  TriggerReceivePdcpPdu (m_keepS0);

                                  /**
                                  * Deliver zero, one or multiple PDUs
                                  */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                  * Keep S0
                                  */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                      default:
                              /**
                                * ERROR: Transition not possible
                                */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          default:
                NS_LOG_LOGIC ("INTERNAL ERROR: Wrong reassembling state = " << (uint32_t) m_reassemblingState);
          break;
        }
    }
  else // Reassemble the list of SDUs (when there are losses, i.e. the received SN is not the expected one)
    {
      switch (m_reassemblingState)
        {
          case WAITING_S0_FULL:
                  switch (framingInfo)
                    {
                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Deliver one or multiple PDUs
                               */
                              for ( it = m_sdusBuffer.begin () ; it != m_sdusBuffer.end () ; it++ )
                                {
                                  TriggerReceivePdcpPdu (*it);
                                }
                              m_sdusBuffer.clear ();
                      break;

                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                               * Deliver full PDUs
                               */
                              while ( m_sdusBuffer.size () > 1 )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }

                              /**
                               * Keep S0
                               */
                              m_keepS0 = m_sdusBuffer.front ();
                              m_sdusBuffer.pop_front ();
                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard SN
                               */
                              m_sdusBuffer.pop_front ();

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_reassemblingState = WAITING_S0_FULL;
                                }
                              else
                                {
                                  m_reassemblingState = WAITING_SI_SF;
                                }

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              if ( m_sdusBuffer.size () > 0 )
                                {
                                  /**
                                  * Deliver zero, one or multiple PDUs
                                  */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                  * Keep S0
                                  */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      default:
                              /**
                               * ERROR: Transition not possible
                               */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          case WAITING_SI_SF:
                  switch (framingInfo)
                    {
                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Deliver one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (NrRlcHeader::FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              m_reassemblingState = WAITING_SI_SF;

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( m_sdusBuffer.size () > 1 )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }

                              /**
                               * Keep S0
                               */
                              m_keepS0 = m_sdusBuffer.front ();
                              m_sdusBuffer.pop_front ();

                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::LAST_BYTE):
                              m_reassemblingState = WAITING_S0_FULL;

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              /**
                               * Deliver zero, one or multiple PDUs
                               */
                              while ( ! m_sdusBuffer.empty () )
                                {
                                  TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      case (NrRlcHeader::NO_FIRST_BYTE | NrRlcHeader::NO_LAST_BYTE):
                              if ( m_sdusBuffer.size () == 1 )
                                {
                                  m_reassemblingState = WAITING_S0_FULL;
                                }
                              else
                                {
                                  m_reassemblingState = WAITING_SI_SF;
                                }

                              /**
                               * Discard S0
                               */
                              m_keepS0 = 0;

                              /**
                               * Discard SI or SN
                               */
                              m_sdusBuffer.pop_front ();

                              if ( m_sdusBuffer.size () > 0 )
                                {
                                  /**
                                   * Deliver zero, one or multiple PDUs
                                   */
                                  while ( m_sdusBuffer.size () > 1 )
                                    {
                                      TriggerReceivePdcpPdu (m_sdusBuffer.front ());
                                      m_sdusBuffer.pop_front ();
                                    }

                                  /**
                                   * Keep S0
                                   */
                                  m_keepS0 = m_sdusBuffer.front ();
                                  m_sdusBuffer.pop_front ();
                                }
                      break;

                      default:
                              /**
                                * ERROR: Transition not possible
                                */
                              NS_LOG_LOGIC ("INTERNAL ERROR: Transition not possible. FI = " << (uint32_t) framingInfo);
                      break;
                    }
          break;

          default:
                NS_LOG_LOGIC ("INTERNAL ERROR: Wrong reassembling state = " << (uint32_t) m_reassemblingState);
          break;
        }
    }

}


void
NrRlcUmLowLat::ReassembleOutsideWindow (void)
{
  NS_LOG_LOGIC ("Reassemble Outside Window");

  std::map <uint16_t, Ptr<Packet> >::iterator it;
  it = m_rxBuffer.begin ();

  while ( (it != m_rxBuffer.end ()) && ! IsInsideReorderingWindow (SequenceNumber10 (it->first)) )
    {
      NS_LOG_LOGIC ("SN = " << it->first);

      // Reassemble RLC SDUs and deliver the PDCP PDU to upper layer
      ReassembleAndDeliver (it->second);

      std::map <uint16_t, Ptr<Packet> >::iterator it_tmp = it;
      ++it;
      m_rxBuffer.erase (it_tmp);
    }

  if (it != m_rxBuffer.end ())
    {
      NS_LOG_LOGIC ("(SN = " << it->first << ") is inside the reordering window");
    }
}

void
NrRlcUmLowLat::ReassembleSnInterval (SequenceNumber10 lowSeqNumber, SequenceNumber10 highSeqNumber)
{
  NS_LOG_LOGIC ("Reassemble SN between " << lowSeqNumber << " and " << highSeqNumber);

  std::map <uint16_t, Ptr<Packet> >::iterator it;

  SequenceNumber10 reassembleSn = lowSeqNumber;
  NS_LOG_LOGIC ("reassembleSN = " << reassembleSn);
  NS_LOG_LOGIC ("highSeqNumber = " << highSeqNumber);
  while (reassembleSn < highSeqNumber)
    {
      NS_LOG_LOGIC ("reassembleSn < highSeqNumber");
      it = m_rxBuffer.find (reassembleSn.GetValue ());
      NS_LOG_LOGIC ("it->first  = " << it->first);
      NS_LOG_LOGIC ("it->second = " << it->second);
      if (it != m_rxBuffer.end () )
        {
          NS_LOG_LOGIC ("SN = " << it->first);

          // Reassemble RLC SDUs and deliver the PDCP PDU to upper layer
          ReassembleAndDeliver (it->second);

          m_rxBuffer.erase (it);
        }
        
      reassembleSn++;
    }
}


void
NrRlcUmLowLat::DoReportBufferStatus (void)
{
  if(!m_bsrReported)
  {
    Time holDelay (0);
    uint32_t queueSize = 0;

    if (! m_txBuffer.empty ())
      {
        RlcTag holTimeTag;
        m_txBuffer.front ()->PeekPacketTag (holTimeTag);
        holDelay = Simulator::Now () - holTimeTag.GetSenderTimestamp ();

        queueSize = m_txBufferSize + 2 * m_txBuffer.size (); // Data in tx queue + estimated headers size
      }

    NrMacSapProvider::ReportBufferStatusParameters r;
    r.rnti = m_rnti;
    r.lcid = m_lcid;
    r.txQueueSize = queueSize;
    r.txQueueHolDelay = holDelay.GetMicroSeconds () ;
    r.retxQueueSize = 0;
    r.retxQueueHolDelay = 0;
    r.statusPduSize = 0;
    m_txBufferSize = queueSize; //sjkang1114
    TxOn_QueingDelay = r.txQueueHolDelay; //sjkang1114
    for (unsigned i = 0; i < m_txBuffer.size(); i++)
    {
      if (i == 20)  // only include up to the first 20 packets
      {
        break;
      }
      r.txPacketSizes.push_back (m_txBuffer[i]->GetSize ());
      RlcTag holTimeTag;
      m_txBuffer[i]->PeekPacketTag (holTimeTag);
      holDelay = Simulator::Now () - holTimeTag.GetSenderTimestamp ();
      r.txPacketDelays.push_back (holDelay.GetMicroSeconds ());
    }

    r.arrivalRate = m_arrivalRate;

    NS_LOG_INFO ("Send ReportBufferStatus = " << r.txQueueSize << ", " << r.txQueueHolDelay << ", " << r.txPacketSizes.size());
    m_macSapProvider->ReportBufferStatus (r);
  }
}


void
NrRlcUmLowLat::ExpireReorderingTimer (void)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid);
  NS_LOG_LOGIC ("Reordering timer has expired");

  // 5.1.2.2.4 Actions when t-Reordering expires
  // When t-Reordering expires, the receiving UM RLC entity shall:
  // - update VR(UR) to the SN of the first UMD PDU with SN >= VR(UX) that has not been received;
  // - reassemble RLC SDUs from any UMD PDUs with SN < updated VR(UR), remove RLC headers when doing so
  //   and deliver the reassembled RLC SDUs to upper layer in ascending order of the RLC SN if not delivered before;
  // - if VR(UH) > VR(UR):
  //    - start t-Reordering;
  //    - set VR(UX) to VR(UH).

  std::map <uint16_t, Ptr<Packet> >::iterator it;
  SequenceNumber10 newVrUr = m_vrUx;

  while ( (it = m_rxBuffer.find (newVrUr.GetValue ())) != m_rxBuffer.end () )
    {
      newVrUr++;
    }
  SequenceNumber10 oldVrUr = m_vrUr;
  m_vrUr = newVrUr;
  NS_LOG_LOGIC ("New VR(UR) = " << m_vrUr);

  ReassembleSnInterval (oldVrUr, m_vrUr);

  if ( m_vrUh > m_vrUr)
    {
      NS_LOG_LOGIC ("Start reordering timer");
      m_reorderingTimer = Simulator::Schedule (m_reorderingTimeExpires,
                                               &NrRlcUmLowLat::ExpireReorderingTimer, this);
      m_vrUx = m_vrUh;
      NS_LOG_LOGIC ("New VR(UX) = " << m_vrUx);
    }
}


void
NrRlcUmLowLat::ExpireRbsTimer (void)
{
  NS_LOG_LOGIC ("RBS Timer expires");

  if (! m_txBuffer.empty ())
    {
      DoReportBufferStatus ();
      m_rbsTimer = Simulator::Schedule (MilliSeconds (10), &NrRlcUmLowLat::ExpireRbsTimer, this);
    }
}


void
NrRlcUmLowLat::TriggerReceivePdcpPdu(Ptr<Packet> p)
{
  if(!isMc) 
  {
    NS_LOG_INFO(this << " RlcUmLowLat forwards packet to PDCP (either from MmWave or NR stack)");
    m_rlcSapUser->ReceivePdcpPdu(p);
  }
  else
  {
    NS_LOG_INFO(this << " MmWave Rlc Um LowLat forwards packet to remote PDCP");
    m_ueDataParams.ueData = p;
    m_ngcX2RlcProvider->ReceiveMcPdcpSdu(m_ueDataParams);
  }
}
void
NrRlcUmLowLat::DoSendAssistantInformation(NgcX2Sap::AssistantInformationForSplitting info){ //sjkang

	info.targetCellId = m_ueDataParams.targetCellId;
	info.sourceCellId = m_ueDataParams.sourceCellId;
    info.Tx_On_Q_Size=m_txBufferSize;
    info.Tx_On_Q_Delay = TxOn_QueingDelay;
    info.rnti = m_rnti;
    info.drbId = (unsigned)m_drbId;
  // m_sendAssistatInfo.Cancel();
	//m_ngcX2RlcProvider->ReceiveAssistantInformation(info);
	//if(!m_sendAssistatInfo.IsRunning())
	    //  m_sendAssistatInfo = Simulator::Schedule(MilliSeconds(1),&NrRlcUmLowLat::DoSendAssistantInformation,this, info);

}
void
NrRlcUmLowLat::CalculatePathThroughput (std::ofstream *streamPathThroughput){
  NS_FATAL_ERROR ("Not implemented yet");
}
void
NrRlcUmLowLat::RecordingQueueStatistics(){//sjkang1116
	*measuringQusizeQueueDelayStream << Simulator::Now().GetSeconds()<<"\t"<<m_txBufferSize << "\t"
			<<TxOn_QueingDelay<< std::endl;
m_reorderingTimer.Cancel();
	if(!m_reorderingQueueStatistic.IsRunning())
		m_reorderingQueueStatistic = Simulator::Schedule(MilliSeconds(1.0),&NrRlcUmLowLat::RecordingQueueStatistics,this);
}
void
NrRlcUmLowLat::DoRequestAssistantInfo(){
	  NgcX2Sap::AssistantInformationForSplitting info;
		            std::cout << "will send NR assistant info from NrRlcUm" <<std::endl;
		            RecordingQueueStatistics();

		            Simulator::Schedule(MilliSeconds(10),&NrRlcUmLowLat::DoSendAssistantInformation,this, info);
}
//voi
//NrRlcUmLowLat::SetDrbId(uint8_t drbId){
//	m_drbId = drbId;
	//std::cout << "settng drbId ----->" << (unsigned)m_drbId << std::endl;
//}
} // namespace ns3

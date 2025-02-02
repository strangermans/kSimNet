/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 */

#ifndef NR_PDCP_HEADER_H
#define NR_PDCP_HEADER_H

#include "ns3/header.h"

#include <list>

namespace ns3 {

/**
 * \ingroup nr
 * \brief The packet header for the Packet Data Convergence Protocol (PDCP) packets
 *
 * This class has fields corresponding to those in an PDCP header as well as
 * methods for serialization to and deserialization from a byte buffer.
 * It follows 3GPP TS 36.323 Packet Data Convergence Protocol (PDCP) specification.
 */
class NrPdcpHeader : public Header
{
public:

  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  NrPdcpHeader ();
  ~NrPdcpHeader ();

  void SetDcBit (uint8_t dcBit);
  void SetSequenceNumber (uint16_t sequenceNumber);
  void SetSourceCellId(uint8_t sourceCellId); //sjkang1116

  uint8_t GetDcBit () const;
  uint16_t GetSequenceNumber () const;
  uint8_t GetSourceCellId() const; //sjkang1116

  typedef enum {
    CONTROL_PDU   = 0,
    DATA_PDU      = 1
  } DcBit_t;

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);


private:
  uint8_t m_dcBit;
  uint16_t m_sequenceNumber;
  uint8_t sourceCellId;

};

} // namespace ns3

#endif // NR_PDCP_HEADER_H

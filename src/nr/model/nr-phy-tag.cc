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
 * Author: Marco Miozzo <marco.miozzo@cttc.es>
 *         Nicola Baldo <nbaldo@cttc.es>
 */


#include "nr-phy-tag.h"
#include "ns3/tag.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (NrPhyTag);

TypeId
NrPhyTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrPhyTag")
    .SetParent<Tag> ()
    .SetGroupName("Nr")
    .AddConstructor<NrPhyTag> ()
  ;
  return tid;
}

TypeId
NrPhyTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

NrPhyTag::NrPhyTag ()
{
}

NrPhyTag::NrPhyTag (uint16_t cellId)
  : m_cellId (cellId)
{
}

NrPhyTag::~NrPhyTag ()
{
}

uint32_t
NrPhyTag::GetSerializedSize (void) const
{
  return 2;
}

void
NrPhyTag::Serialize (TagBuffer i) const
{
  i.WriteU16 (m_cellId);
}

void
NrPhyTag::Deserialize (TagBuffer i)
{
  m_cellId = i.ReadU16 ();
}

void
NrPhyTag::Print (std::ostream &os) const
{
  os << m_cellId;
}

uint16_t
NrPhyTag::GetCellId () const
{
  return m_cellId;
}

} // namespace ns3

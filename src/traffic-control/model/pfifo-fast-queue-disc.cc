/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2014 University of Washington
 *               2015 Universita' degli Studi di Napoli Federico II
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
 * Authors:  Stefano Avallone <stavallo@unina.it>
 *           Tom Henderson <tomhend@u.washington.edu>
 */

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/socket.h"
#include "pfifo-fast-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PfifoFastQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (PfifoFastQueueDisc);

TypeId PfifoFastQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PfifoFastQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PfifoFastQueueDisc> ()
    .AddAttribute ("Limit",
                   "The maximum number of packets accepted by this queue disc.",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&PfifoFastQueueDisc::m_limit),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

PfifoFastQueueDisc::PfifoFastQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

PfifoFastQueueDisc::~PfifoFastQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

const uint32_t PfifoFastQueueDisc::prio2band[16] = {1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};

bool
PfifoFastQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  if (GetNPackets () > m_limit)
    {
      NS_LOG_LOGIC ("Queue disc limit exceeded -- dropping packet");
      Drop (item);
      return false;
    }

  uint8_t priority = 0;
  SocketPriorityTag priorityTag;
  if (item->GetPacket ()->PeekPacketTag (priorityTag))
    {
      priority = priorityTag.GetPriority ();
    }

  uint32_t band = prio2band[priority & 0x0f];

  bool retval = GetInternalQueue (band)->Enqueue (item);
  // std::cout << retval << std::endl;
  // If Queue::Enqueue fails, QueueDisc::Drop is called by the internal queue
  // because QueueDisc::AddInternalQueue sets the drop callback

NS_LOG_LOGIC ("Number packets band " << band << ": " << GetInternalQueue (band)->GetNPackets ());

  return retval;
}

Ptr<QueueDiscItem>
PfifoFastQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Dequeue ()) != 0)
        {
          NS_LOG_LOGIC ("Popped from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }
  
  NS_LOG_INFO ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
PfifoFastQueueDisc::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
PfifoFastQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs no packet filter");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create 3 DropTail queues with m_limit packets each
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
      factory.Set ("Mode", EnumValue (QueueBase::QUEUE_MODE_PACKETS));
      factory.Set ("MaxPackets", UintegerValue (m_limit));
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
    }

  if (GetNInternalQueues () != 3)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs 3 internal queues");
      return false;
    }

  if (GetInternalQueue (0)-> GetMode () != QueueBase::QUEUE_MODE_PACKETS ||
      GetInternalQueue (1)-> GetMode () != QueueBase::QUEUE_MODE_PACKETS ||
      GetInternalQueue (2)-> GetMode () != QueueBase::QUEUE_MODE_PACKETS)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs 3 internal queues operating in packet mode");
      return false;
    }

  for (uint8_t i = 0; i < 2; i++)
    {
      if (GetInternalQueue (i)->GetMaxPackets () < m_limit)
        {
          NS_LOG_ERROR ("The capacity of some internal queue(s) is less than the queue disc capacity");
          return false;
        }
    }

  return true;
}

void
PfifoFastQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#include "ns3/abort.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "queue.h"
//#include "ns3/queue-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Queue");

NS_OBJECT_ENSURE_REGISTERED (QueueBase);
NS_OBJECT_TEMPLATE_CLASS_DEFINE (Queue,Packet);
NS_OBJECT_TEMPLATE_CLASS_DEFINE (Queue,QueueItem);

TypeId
QueueBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QueueBase")
    .SetParent<Object> ()
    .SetGroupName ("Network")
    .AddAttribute ("Mode",
                   "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&QueueBase::SetMode,
                                     &QueueBase::GetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets accepted by this queue.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&QueueBase::SetMaxPackets,
                                         &QueueBase::GetMaxPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBytes",
                   "The maximum number of bytes accepted by this queue.",
                   UintegerValue (100 * 65535),
                   MakeUintegerAccessor (&QueueBase::SetMaxBytes,
                                         &QueueBase::GetMaxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("PacketsInQueue",
                     "Number of packets currently stored in the queue",
                     MakeTraceSourceAccessor (&QueueBase::m_nPackets),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("BytesInQueue",
                     "Number of bytes currently stored in the queue",
                     MakeTraceSourceAccessor (&QueueBase::m_nBytes),
                     "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

QueueBase::QueueBase () :
  m_nBytes (0),
  m_nTotalReceivedBytes (0),
  m_nPackets (0),
  m_nTotalReceivedPackets (0),
  m_nTotalDroppedBytes (0),
  m_nTotalDroppedBytesBeforeEnqueue (0),
  m_nTotalDroppedBytesAfterDequeue (0),
  m_nTotalDroppedPackets (0),
  m_nTotalDroppedPacketsBeforeEnqueue (0),
  m_nTotalDroppedPacketsAfterDequeue (0),
  m_mode (QUEUE_MODE_PACKETS)
{
  NS_LOG_FUNCTION (this);
}

QueueBase::~QueueBase ()
{
  NS_LOG_FUNCTION (this);
}

void
QueueBase::AppendItemTypeIfNotPresent (std::string& typeId, const std::string& itemType)
{
  if (typeId.back () != '>')
    {
      typeId.append ("<" + itemType + ">");
    }
}

bool
QueueBase::IsEmpty (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << (m_nPackets.Get () == 0));
  return m_nPackets.Get () == 0;
}

uint32_t
QueueBase::GetNPackets (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nPackets);
  return m_nPackets;
}

uint32_t
QueueBase::GetNBytes (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (" returns " << m_nBytes);
  return m_nBytes;
}

uint32_t
QueueBase::GetTotalReceivedBytes (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalReceivedBytes);
  return m_nTotalReceivedBytes;
}

uint32_t
QueueBase::GetTotalReceivedPackets (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalReceivedPackets);
  return m_nTotalReceivedPackets;
}

uint32_t
QueueBase:: GetTotalDroppedBytes (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalDroppedBytes);
  return m_nTotalDroppedBytes;
}

uint32_t
QueueBase:: GetTotalDroppedBytesBeforeEnqueue (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalDroppedBytesBeforeEnqueue);
  return m_nTotalDroppedBytesBeforeEnqueue;
}

uint32_t
QueueBase:: GetTotalDroppedBytesAfterDequeue (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalDroppedBytesAfterDequeue);
  return m_nTotalDroppedBytesAfterDequeue;
}

uint32_t
QueueBase::GetTotalDroppedPackets (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalDroppedPackets);
  return m_nTotalDroppedPackets;
}

uint32_t
QueueBase::GetTotalDroppedPacketsBeforeEnqueue (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalDroppedPacketsBeforeEnqueue);
  return m_nTotalDroppedPacketsBeforeEnqueue;
}

uint32_t
QueueBase::GetTotalDroppedPacketsAfterDequeue (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalDroppedPacketsAfterDequeue);
  return m_nTotalDroppedPacketsAfterDequeue;
}

void
QueueBase::ResetStatistics (void)
{
  NS_LOG_FUNCTION (this);
  m_nTotalReceivedBytes = 0;
  m_nTotalReceivedPackets = 0;
  m_nTotalDroppedBytes = 0;
  m_nTotalDroppedBytesBeforeEnqueue = 0;
  m_nTotalDroppedBytesAfterDequeue = 0;
  m_nTotalDroppedPackets = 0;
  m_nTotalDroppedPacketsBeforeEnqueue = 0;
  m_nTotalDroppedPacketsAfterDequeue = 0;
}

void
QueueBase::SetMode (QueueBase::QueueMode mode)
{
  NS_LOG_FUNCTION (this << mode);

  if (mode == QUEUE_MODE_BYTES && m_mode == QUEUE_MODE_PACKETS)
    {
      NS_ABORT_MSG_IF (m_nPackets.Get () != 0,
                       "Cannot change queue mode in a queue with packets.");
    }
  else if (mode == QUEUE_MODE_PACKETS && m_mode == QUEUE_MODE_BYTES)
    {
      NS_ABORT_MSG_IF (m_nBytes.Get () != 0,
                       "Cannot change queue mode in a queue with packets.");
    }

  m_mode = mode;
}

QueueBase::QueueMode
QueueBase::GetMode (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

void
QueueBase::SetMaxPackets (uint32_t maxPackets)
{
  NS_LOG_FUNCTION (this << maxPackets);

  if (m_mode == QUEUE_MODE_PACKETS)
    {
      NS_ABORT_MSG_IF (maxPackets < m_nPackets.Get (),
                       "The new queue size cannot be less than the number of currently stored packets.");
    }

  m_maxPackets = maxPackets;
}

uint32_t
QueueBase::GetMaxPackets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_maxPackets;
}

void
QueueBase::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);

  if (m_mode == QUEUE_MODE_BYTES)
    {
      NS_ABORT_MSG_IF (maxBytes < m_nBytes.Get (),
                       "The new queue size cannot be less than the amount of bytes of currently stored packets.");
    }

  m_maxBytes = maxBytes;
}

uint32_t
QueueBase::GetMaxBytes (void) const
{
  NS_LOG_FUNCTION (this);
  return m_maxBytes;
}

void
QueueBase::DoNsLog (const enum LogLevel level, std::string str) const
{
    NS_LOG (level, str);
}

/* jhlim for Packet */
/*
template <typename Item>
bool
Queue<Item>::DoEnqueue (Ptr<Item> item, int dummy)
{
	std::cout << "wrong func." << std::endl;
	exit(1);
}
template <typename Item>
Ptr<Item> Queue<Item>::DoDequeue (int dummy)
{
	std::cout << "wrong func." << std::endl;
	exit(1);
}
template <typename Item>
Ptr<Item> Queue<Item>::DoRemove (int dummy)
{
	std::cout << "wrong func." << std::endl;
	exit(1);
}
template <typename Item>
Ptr<const Item> Queue<Item>::DoPeek (int dummy) 
{
	std::cout << "wrong func." << std::endl;
	exit(1);
}
template <typename Item>
bool Queue<Item>::Enqueue (Ptr<Item> item)
{
	exit(1);
}
template <typename Item>
Ptr<Item> Queue<Item>::Dequeue ()
{
	exit(1);
}
template <typename Item>
Ptr<Item> Queue<Item>::Remove (void)
{
	exit(1);
}
*/

/* jhlim for QueueItem */
/*
template <typename Item>
bool
Queue<Item>::Enqueue1 (Ptr<Item> item, int dummy)
{
	//NS_LOG_FUNCTION (this << item);
	if (m_mode ==   QUEUE_MODE_PACKETS && (m_nPackets.Get () >= m_maxPackets))
	{
	      //NS_LOG_LOGIC ("Queue full (at amx packets) -- dropping pkt");
	      Drop1 (item, dummy);
          return false;
    }

	if (m_mode == QUEUE_MODE_BYTES && (m_nBytes.Get () + item->GetSize () > m_maxBytes))
	{
	      //NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- dropping pkt");
	      Drop1 (item, dummy);
	      return false;
	}

	bool retval = DoEnqueue (item, dummy);
	if (retval)
	{
	      //NS_LOG_LOGIC ("m_traceEnqueue (p)");
	      m_traceEnqueue (item->GetPacket ());

	      uint32_t size = item->GetSize ();
	      m_nBytes += size;
	      m_nTotalReceivedBytes += size;

	      m_nPackets++;
	      m_nTotalReceivedPackets++;
	 }
	 return retval;
}

// jhlim: for QueueItem
template <typename Item>
Ptr<Item>
Queue<Item>::Dequeue1 (int dummy)
{
    //NS_LOG_FUNCTION (this);

    if (m_nPackets.Get () == 0)
    {
        NS_LOG_LOGIC ("Queue empty");
        return 0;
    }

    Ptr<Item> item = DoDequeue (dummy);

    if (item != 0)
    {
        //NS_ASSERT (m_nBytes.Get () >= item->GetSize ());
	    //NS_ASSERT (m_nPackets.Get () > 0);

	    m_nBytes -= item->GetSize ();
	    m_nPackets--;

	    //NS_LOG_LOGIC ("m_traceDequeue (packet)");
	    m_traceDequeue (item->GetPacket ());
	}
	    return item;
}

// jhlim: for QueueItem
template <typename Item>
Ptr<Item>
Queue<Item>::Remove1 (int dummy)
{
	//NS_LOG_FUNCTION (this);

	if (m_nPackets.Get () == 0)
	{
	     //NS_LOG_LOGIC ("Queue empty");
	     return 0;
	}

	Ptr<Item> item = DoRemove (dummy);

	if (item != 0)
	{
	     //NS_ASSERT (m_nBytes.Get () >= item->GetSize ());
	     //NS_ASSERT (m_nPackets.Get () > 0);

	     m_nBytes -= item->GetSize ();
	     m_nPackets--;

	     Drop1 (item, dummy);
	}
	return item;
}

// jhlim: for QueueItem
template <typename Item>
Ptr<const Item>
Queue<Item>::Peek1 (int dummy)
{
	//NS_LOG_FUNCTION (this);

	if (m_nPackets.Get () == 0)
	{
	     //NS_LOG_LOGIC ("Queue empty");
	     return 0;
	}
	return DoPeek (dummy);
}

// jhlim: for QueueItem
template <typename Item>
void
Queue<Item>::NotifyDrop (Ptr<Item> item, int dummy)
{
    //NS_LOG_FUNCTION (this << item);

    if (!m_dropCallback.IsNull ())
    {
         m_dropCallback (item);
    }
}


// jhlim: for QueueItem
template <typename Item>
void
Queue<Item>::Drop1 (Ptr<Item> item, int dummy)
{
	//NS_LOG_FUNCTION (this << item);

	m_nTotalDroppedPackets++;
	//m_nTotalDroppedBytes += item->GetPacketSize ();
	m_nTotalDroppedBytes += item->GetSize ();
	 
	//NS_LOG_LOGIC ("m_traceDrop (p)");
	//m_traceDrop (item->GetPacket ());
	m_traceDrop (item);
	NotifyDrop (item, dummy);
}
*/

} // namespace ns3

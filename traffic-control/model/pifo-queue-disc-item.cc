/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Stanford University
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
 * Author: Stephen Ibanez <sibanez@stanford.edu>
 */

#include "ns3/log.h"
#include "pifo-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PifoQueueDiscItem");

PifoQueueDiscItem::PifoQueueDiscItem (Ptr<Packet> p, const Address& addr,
                                      uint16_t protocol, uint32_t priority)
  : QueueDiscItem (p, addr, protocol),
    m_priority (priority)
{
}

PifoQueueDiscItem::PifoQueueDiscItem (Ptr<QueueDiscItem> item, uint32_t priority)
  : QueueDiscItem (item->GetPacket (), item->GetAddress (), item->GetProtocol ()),
    m_priority (priority)
{
}

PifoQueueDiscItem::~PifoQueueDiscItem ()
{
  NS_LOG_FUNCTION (this);
}

uint32_t
PifoQueueDiscItem::GetPriority (void) const
{
  return m_priority;
}

void
PifoQueueDiscItem::SetPriority (uint32_t priority)
{
  NS_LOG_FUNCTION (this);

  m_priority = priority;
}

void
PifoQueueDiscItem::AddHeader (void)
{
}

bool
PifoQueueDiscItem::Mark (void)
{
  return false;
}

void
PifoQueueDiscItem::Print (std::ostream& os) const
{

  os << GetPacket () << " "
     << "Priority " << GetPriority ();
}

} // namespace ns3

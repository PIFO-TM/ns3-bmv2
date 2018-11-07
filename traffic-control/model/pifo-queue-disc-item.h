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

#ifndef PIFO_QUEUE_DISC_ITEM_H
#define PIFO_QUEUE_DISC_ITEM_H

#include "ns3/packet.h"
#include "ns3/queue-item.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * PifoQueueDiscItem is a subclass of QueueDiscItem which stores items for the
 * PifoQueueDisc. Each item stores a priority to be used for scheduling within
 * the PIFO. The priority is stored here rather than as a packet tag to avoid
 * the overhead of serializing and deserializing for each comparison.
 */
class PifoQueueDiscItem : public QueueDiscItem {
public:
  /**
   * \brief Create an PIFO queue disc item.
   * \param p the packet included in the created item.
   * \param addr the destination MAC address
   * \param protocol the protocol number
   * \param priority the packet priority
   */
  PifoQueueDiscItem (Ptr<Packet> p, const Address & addr, uint16_t protocol, uint32_t priority);

  /**
   * \brief Create an PIFO queue disc item using an existing QueueDiscItem ptr
   * \param item a pointer to an existing queue disc item
   * \param priority the packet priority
   */
  PifoQueueDiscItem (Ptr<QueueDiscItem> item, uint32_t priority);

  virtual ~PifoQueueDiscItem ();

  /**
   * \return the item's priority
   */
  uint32_t GetPriority (void) const;

  /**
   * \brief Set the priority of the item.
   */
  void SetPriority (uint32_t priority);

  /**
   * \brief Unused method
   */
  virtual void AddHeader (void);

  /**
   * \brief Unused method
   *
   * \return false always
   */
  virtual bool Mark (void);

  /**
   * \brief Print the item contents.
   * \param os output stream in which the data should be printed.
   */
  virtual void Print (std::ostream &os) const;

private:
  /**
   * \brief Default constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  PifoQueueDiscItem ();
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  PifoQueueDiscItem (const PifoQueueDiscItem &);
  /**
   * \brief Assignment operator
   *
   * Defined and unimplemented to avoid misuse
   */
  PifoQueueDiscItem &operator = (const PifoQueueDiscItem &);

  uint32_t m_priority;  //!< The priority of the item
};

} // namespace ns3

#endif /* PIFO_QUEUE_DISC_ITEM_H */

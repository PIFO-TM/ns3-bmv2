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

#ifndef PRIO_QUEUE_H
#define PRIO_QUEUE_H

#include "ns3/packet.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/traced-value.h"
#include "ns3/unused.h"
#include "ns3/log.h"
#include "ns3/queue-size.h"
#include "ns3/queue.h"
#include <string>
#include <sstream>
#include <list>
#include <queue>

namespace ns3 {

/**
 * \ingroup network
 * \defgroup queue Queue
 */

/**
 * \ingroup queue
 * \brief Template class for packet Priority Queues
 *
 * PrioQueue is a template class. The type of the objects stored within the priority
 * queue is specified by the type parameter, which can be any class providing a
 * GetSize () method and a GetPriority() method (e.g. QueueDiscItem).
 *
 * TODO: evaluate performance overhead of using std::priority_queue rather than std::list
 *
 * Users of the PrioQueue template class usually hold a queue through a smart pointer,
 * hence forward declaration is recommended to avoid pulling the implementation
 * of the templates included in this file. Thus, do not include prio-queue.h but add
 * the following forward declaration in your .h file:
 *
 * \code
 *   template <typename Item> class PrioQueue;
 * \endcode
 *
 * Then, include prio-queue.h in the corresponding .cc file.
 */
template <typename Item>
class PrioQueue : public QueueBase
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  PrioQueue ();
  virtual ~PrioQueue ();

  /**
   * Place an item into the PrioQueue 
   * \param item item to enqueue
   * \return True if the operation was successful; false otherwise
   */
  bool Enqueue (Ptr<Item> item);

  /**
   * Remove an item from the PrioQueue, counting it as dequeued
   * \return 0 if the operation was not successful; the item otherwise.
   */
  Ptr<Item> Dequeue (void);

  /**
   * Remove an item from the PrioQueue, counting it as dropped
   * \return 0 if the operation was not successful; the item otherwise.
   */
  Ptr<Item>  Remove (void);

  /**
   * Get a copy of an item in the PrioQueue without removing it
   * \return 0 if the operation was not successful; the item otherwise.
   */
  Ptr<const Item> Peek (void) const;

  /**
   * Flush the PrioQueue.
   */
  void Flush (void);

protected:

  /**
   * \brief Drop a packet before enqueue
   * \param item item that was dropped
   *
   * This method is called by the base class when a packet is dropped because
   * the queue is full and by the subclasses to notify parent (this class) that
   * a packet has been dropped for other reasons before being enqueued.
   */
  void DropBeforeEnqueue (Ptr<Item> item);

  /**
   * \brief Drop a packet after dequeue
   * \param item item that was dropped
   *
   * This method is called by the base class when a Remove operation is requested
   * and by the subclasses to notify parent (this class) that a packet has been
   * dropped for other reasons after being dequeued.
   */
  void DropAfterDequeue (Ptr<Item> item);

private:
  /**
   * \brief The functor used to compare queue elements based on priority.
   */
  struct ItemComp {
    bool operator()(const Ptr<Item> &lhs, const Ptr<Item> &rhs) const {
      return lhs->GetPriority() >= rhs->GetPriority();
    }
  };

  // Type of the priority queue
  using MyPrioQueue = std::priority_queue<Ptr<Item>, std::deque<Ptr<Item>>, ItemComp >;

  MyPrioQueue m_items;                 //!< the items in the PrioQueue
  NS_LOG_TEMPLATE_DECLARE;             //!< the log component

  /// Traced callback: fired when a packet is enqueued
  TracedCallback<Ptr<const Item> > m_traceEnqueue;
  /// Traced callback: fired when a packet is dequeued
  TracedCallback<Ptr<const Item> > m_traceDequeue;
  /// Traced callback: fired when a packet is dropped
  TracedCallback<Ptr<const Item> > m_traceDrop;
  /// Traced callback: fired when a packet is dropped before enqueue
  TracedCallback<Ptr<const Item> > m_traceDropBeforeEnqueue;
  /// Traced callback: fired when a packet is dropped after dequeue
  TracedCallback<Ptr<const Item> > m_traceDropAfterDequeue;
};


/**
 * Implementation of the templates declared above.
 */

template <typename Item>
TypeId
PrioQueue<Item>::GetTypeId (void)
{
  std::string name = GetTypeParamName<PrioQueue<Item> > ();
  static TypeId tid = TypeId (("ns3::PrioQueue<" + name + ">").c_str ())
    .SetParent<QueueBase> ()
    .SetGroupName ("Network")
    .AddTraceSource ("Enqueue", "Enqueue a packet in the queue.",
                     MakeTraceSourceAccessor (&PrioQueue<Item>::m_traceEnqueue),
                     "ns3::" + name + "::TracedCallback")
    .AddTraceSource ("Dequeue", "Dequeue a packet from the queue.",
                     MakeTraceSourceAccessor (&PrioQueue<Item>::m_traceDequeue),
                     "ns3::" + name + "::TracedCallback")
    .AddTraceSource ("Drop", "Drop a packet (for whatever reason).",
                     MakeTraceSourceAccessor (&PrioQueue<Item>::m_traceDrop),
                     "ns3::" + name + "::TracedCallback")
    .AddTraceSource ("DropBeforeEnqueue", "Drop a packet before enqueue.",
                     MakeTraceSourceAccessor (&PrioQueue<Item>::m_traceDropBeforeEnqueue),
                     "ns3::" + name + "::TracedCallback")
    .AddTraceSource ("DropAfterDequeue", "Drop a packet after dequeue.",
                     MakeTraceSourceAccessor (&PrioQueue<Item>::m_traceDropAfterDequeue),
                     "ns3::" + name + "::TracedCallback")
    .template AddConstructor<PrioQueue<Item> > ()
  ;
  return tid;
}

template <typename Item>
PrioQueue<Item>::PrioQueue ()
  : NS_LOG_TEMPLATE_DEFINE ("PrioQueue")
{
  NS_LOG_FUNCTION(this);
}

template <typename Item>
PrioQueue<Item>::~PrioQueue ()
{
  NS_LOG_FUNCTION(this);
}

template <typename Item>
bool
PrioQueue<Item>::Enqueue (Ptr<Item> item)
{
  NS_LOG_FUNCTION (this << item);

  if (GetCurrentSize () + item > GetMaxSize ())
    {
      NS_LOG_LOGIC ("PrioQueue full -- dropping pkt");
      DropBeforeEnqueue (item);
      return false;
    }

  m_items.push(item);

  uint32_t size = item->GetSize ();
  m_nBytes += size;
  m_nTotalReceivedBytes += size;

  m_nPackets++;
  m_nTotalReceivedPackets++;

  NS_LOG_LOGIC ("m_traceEnqueue (p)");
  m_traceEnqueue (item);

  return true;
}

template <typename Item>
Ptr<Item>
PrioQueue<Item>::Dequeue ()
{
  NS_LOG_FUNCTION (this);

  if (m_nPackets.Get () == 0)
    {
      NS_LOG_LOGIC ("PrioQueue empty");
      return 0;
    }

  Ptr<Item> item = m_items.top();
  m_items.pop();

  if (item != 0)
    {
      NS_ASSERT (m_nBytes.Get () >= item->GetSize ());
      NS_ASSERT (m_nPackets.Get () > 0);

      m_nBytes -= item->GetSize ();
      m_nPackets--;

      NS_LOG_LOGIC ("m_traceDequeue (p)");
      m_traceDequeue (item);
    }
  return item;
}

template <typename Item>
Ptr<Item>
PrioQueue<Item>::Remove ()
{
  NS_LOG_FUNCTION (this);

  if (m_nPackets.Get () == 0)
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<Item> item = m_items.top();
  m_items.pop();

  if (item != 0)
    {
      NS_ASSERT (m_nBytes.Get () >= item->GetSize ());
      NS_ASSERT (m_nPackets.Get () > 0);

      m_nBytes -= item->GetSize ();
      m_nPackets--;

      // packets are first dequeued and then dropped
      NS_LOG_LOGIC ("m_traceDequeue (p)");
      m_traceDequeue (item);

      DropAfterDequeue (item);
    }
  return item;
}

template <typename Item>
void
PrioQueue<Item>::Flush (void)
{
  NS_LOG_FUNCTION (this);
  while (!IsEmpty ())
    {
      Remove ();
    }
}

template <typename Item>
Ptr<const Item>
PrioQueue<Item>::Peek () const
{
  NS_LOG_FUNCTION (this);

  if (m_nPackets.Get () == 0)
    {
      NS_LOG_LOGIC ("PrioQueue empty");
      return 0;
    }

  return m_items.top();
}

template <typename Item>
void
PrioQueue<Item>::DropBeforeEnqueue (Ptr<Item> item)
{
  NS_LOG_FUNCTION (this << item);

  m_nTotalDroppedPackets++;
  m_nTotalDroppedPacketsBeforeEnqueue++;
  m_nTotalDroppedBytes += item->GetSize ();
  m_nTotalDroppedBytesBeforeEnqueue += item->GetSize ();

  NS_LOG_LOGIC ("m_traceDropBeforeEnqueue (p)");
  m_traceDrop (item);
  m_traceDropBeforeEnqueue (item);
}

template <typename Item>
void
PrioQueue<Item>::DropAfterDequeue (Ptr<Item> item)
{
  NS_LOG_FUNCTION (this << item);

  m_nTotalDroppedPackets++;
  m_nTotalDroppedPacketsAfterDequeue++;
  m_nTotalDroppedBytes += item->GetSize ();
  m_nTotalDroppedBytesAfterDequeue += item->GetSize ();

  NS_LOG_LOGIC ("m_traceDropAfterDequeue (p)");
  m_traceDrop (item);
  m_traceDropAfterDequeue (item);
}

} // namespace ns3

#endif /* QUEUE_H */

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
 * Authors: Stephen Ibanez <sibanez@stanford.edu>
 *
 */

#include "ns3/test.h"
#include "ns3/pifo-queue-disc.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/packet-filter.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <array>
#include <queue>

using namespace ns3;

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Queue Disc Test Item
 */
class PifoQueueDiscTestItem : public QueueDiscItem
{
public:
  /**
   * Constructor
   *
   * \param p the packet
   * \param addr the address
   */
  PifoQueueDiscTestItem (Ptr<Packet> p, const Address & addr);
  virtual ~PifoQueueDiscTestItem ();
  virtual void AddHeader (void);
  virtual bool Mark (void);
};

PifoQueueDiscTestItem::PifoQueueDiscTestItem (Ptr<Packet> p, const Address & addr)
  : QueueDiscItem (p, addr, 0)
{
}

PifoQueueDiscTestItem::~PifoQueueDiscTestItem ()
{
}

void
PifoQueueDiscTestItem::AddHeader (void)
{
}

bool
PifoQueueDiscTestItem::Mark (void)
{
  return false;
}


/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Queue Disc Test Packet Filter
 */
class PifoQueueDiscTestFilter : public PacketFilter
{
public:
  /**
   * Constructor
   *
   * \param cls whether this filter is able to classify a PifoQueueDiscTestItem
   */
  PifoQueueDiscTestFilter (bool cls);
  virtual ~PifoQueueDiscTestFilter ();
  /**
   * \brief Set the value returned by DoClassify
   *
   * \param ret the value that DoClassify returns
   */
  void SetReturnValue (int32_t ret);

private:
  virtual bool CheckProtocol (Ptr<QueueDiscItem> item) const;
  virtual int32_t DoClassify (Ptr<QueueDiscItem> item) const;

  bool m_cls;     //!< whether this filter is able to classify a PifoQueueDiscTestItem
  int32_t m_ret;  //!< the value that DoClassify returns if m_cls is true
};

PifoQueueDiscTestFilter::PifoQueueDiscTestFilter (bool cls)
  : m_cls (cls),
    m_ret (0)
{
}

PifoQueueDiscTestFilter::~PifoQueueDiscTestFilter ()
{
}

void
PifoQueueDiscTestFilter::SetReturnValue (int32_t ret)
{
  m_ret = ret;
}

bool
PifoQueueDiscTestFilter::CheckProtocol (Ptr<QueueDiscItem> item) const
{
  return m_cls;
}

int32_t
PifoQueueDiscTestFilter::DoClassify (Ptr<QueueDiscItem> item) const
{
  return m_ret;
}


/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Queue Disc Test Case
 */
class PifoQueueDiscTestCase : public TestCase
{
public:
  PifoQueueDiscTestCase ();
  virtual void DoRun (void);
};

PifoQueueDiscTestCase::PifoQueueDiscTestCase ()
  : TestCase ("Sanity check on the pifo queue disc implementation")
{
}

class PifoItem
{
public:
  PifoItem (uint64_t uid, uint32_t priority);
  ~PifoItem ();
  uint64_t GetUid (void);
  int32_t GetPriority (void);
private:
  uint64_t m_uid;
  int32_t m_priority;
};

PifoItem::PifoItem (uint64_t uid, int32_t priority)
  : m_uid (uid), m_priority(priority)
{
}

PifoItem::~PifoItem ()
{
}

uint64_t
PifoItem::GetUid (void)
{
  return m_uid;
}

int32_t
PifoItem::GetPriority (void)
{
  return m_priority;
}

struct PifoCmp {
  bool operator()(const PifoItem &lhs, const PifoItem &rhs) const {
    return lhs.GetPriority() >= rhs.GetPriority();
  }
};

// Type of the priority queue to sort pkt uids
using MyPrioQueue = std::priority_queue<PifoItem, std::deque<PifoItem>, PifoCmp>;

void
PifoQueueDiscTestCase::DoRun (void)
{
  Ptr<PifoQueueDisc> qdisc;
  Ptr<QueueDiscItem> item;
  Address dest;
  MyPrioQueue uid_pifo;

  // create PifoQueueDisc
  qdisc = CreateObject<PifoQueueDisc> ();
  qdisc->Initialize ();

  NS_TEST_EXPECT_MSG_EQ (qdisc->GetNInternalPrioQueues (), 1, "Verify that there is a single internal priority queue");

  /*
   * Test 1: enqueue one packet
   */
  NS_TEST_EXPECT_MSG_EQ (qdisc->GetNPackets (),
                         0, "There should be no packets in the queue disc");

  item = Create<PifoQueueDiscTestItem> (Create<Packet> (100), dest);
  // add filter
  Ptr<PifoQueueDiscTestFilter> pf1 = CreateObject<PifoQueueDiscTestFilter> (true);
  int32_t rank = 10;
  pf1->SetReturnValue(rank);
  qdisc->AddPacketFilter (pf1);
  // insert pkt
  qdisc->Enqueue (item);
  uid_pifo.emplace(item->GetPacket ()->GetUid (), rank)

  NS_TEST_EXPECT_MSG_EQ (qdisc->->GetNPackets (),
                         1, "There should be one packet in the queue disc");

  /*
   * Test 2: dequeue packets 
   */

  PifoItem uid_item;
  uint64_t actual, expected;
  // NOTE: here item is no longer a Ptr<PifoQueueDiscTestItem>, it is a Ptr<PifoQueueDiscItem>
  while ((item = qdisc->Dequeue ()))
    {
      NS_TEST_ASSERT_MSG_NE(uid_pifo.size(), 0, "The uid_pifo should not be empty yet");
      uid_item = uid_pifo.top();
      uid_pifo.pop();

      actual = item->GetPacket ()->GetUid ();
      expected = uid_item.GetUid();

      NS_TEST_ASSERT_MSG_EQ(actual, expected, "The actual UID " << actual << " does not match the expected UID " << expected);
    }

  Simulator::Destroy ();
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Queue Disc Test Suite
 */
static class PifoQueueDiscTestSuite : public TestSuite
{
public:
  PifoQueueDiscTestSuite ()
    : TestSuite ("pifo-queue-disc", UNIT)
  {
    AddTestCase (new PifoQueueDiscTestCase (), TestCase::QUICK);
  }
} g_pifoQueueTestSuite; ///< the test suite

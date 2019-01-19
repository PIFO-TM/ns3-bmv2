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
#include "ns3/pifo-tree-queue-disc.h"
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
 * \brief Pifo Tree Queue Disc Test Item
 */
class PifoTreeQueueDiscTestItem : public QueueDiscItem
{
public:
  /**
   * Constructor
   *
   * \param p the packet
   * \param addr the address
   */
  PifoTreeQueueDiscTestItem (Ptr<Packet> p, const Address & addr);
  virtual ~PifoTreeQueueDiscTestItem ();
  virtual void AddHeader (void);
  virtual bool Mark (void);
};

PifoTreeQueueDiscTestItem::PifoTreeQueueDiscTestItem (Ptr<Packet> p, const Address & addr)
  : QueueDiscItem (p, addr, 0)
{
}

PifoTreeQueueDiscTestItem::~PifoTreeQueueDiscTestItem ()
{
}

void
PifoTreeQueueDiscTestItem::AddHeader (void)
{
}

bool
PifoTreeQueueDiscTestItem::Mark (void)
{
  return false;
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Tree Queue Disc Test Case
 */
class PifoTreeQueueDiscTestCase : public TestCase
{
public:
  PifoTreeQueueDiscTestCase ();
  virtual void DoRun (void);
};

PifoTreeQueueDiscTestCase::PifoTreeQueueDiscTestCase ()
  : TestCase ("Sanity check on the pifo tree queue disc implementation")
{
}

struct PifoItem {
  PifoItem(uint64_t uid, uint32_t priority)
      : m_uid(uid), m_priority(priority) { }

  uint64_t m_uid;
  int32_t m_priority;
};

struct PifoCmp {
  bool operator()(const PifoItem &lhs, const PifoItem &rhs) const {
    return lhs.m_priority >= rhs.m_priority;
  }
};

// Type of the priority queue to sort pkt uids
using MyPrioQueue = std::priority_queue<PifoItem, std::deque<PifoItem>, PifoCmp>;

void
PifoTreeQueueDiscTestCase::DoRun (void)
{
  Ptr<PifoTreeQueueDisc> qdisc;
  Ptr<QueueDiscItem> item;
  Address dest;
  MyPrioQueue uid_pifo;

  // create PifoTreeQueueDisc
  qdisc = CreateObject<PifoTreeQueueDisc> ();

  // configure with PifoTree JSON file 
  NS_TEST_EXPECT_MSG_EQ (qdisc->SetAttributeFailSafe ("JsonFile", StringValue ("/home/sibanez/tools/bake/source/ns-3.29/src/traffic-control/test/p4-src/test2/pifo-tree.json")), true,
                         "Verify that we can actually set the JsonFile attribute");
  int rank = 10;

  qdisc->Initialize ();

  /*
   * Test 1: enqueue one packet
   */
  NS_TEST_EXPECT_MSG_EQ (qdisc->GetNPackets (), 0, "There should be no packets in the queue disc");

  item = Create<PifoTreeQueueDiscTestItem> (Create<Packet> (100), dest);
  // insert pkt
  qdisc->Enqueue (item);
  uid_pifo.emplace(item->GetPacket ()->GetUid (), rank);

  NS_TEST_EXPECT_MSG_EQ (qdisc->GetNPackets (), 1, "There should be one packet in the queue disc");

  /*
   * Test 2: dequeue packets 
   */

  uint64_t actual, expected;
  while ((item = qdisc->Dequeue ()))
    {
      NS_TEST_ASSERT_MSG_NE(uid_pifo.size(), 0, "The uid_pifo should not be empty yet");

      actual = item->GetPacket ()->GetUid ();
      expected = uid_pifo.top().m_uid;

      NS_TEST_ASSERT_MSG_EQ(actual, expected, "The actual UID " << actual << " does not match the expected UID " << expected);

      uid_pifo.pop();
    }

  Simulator::Destroy ();
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Pifo Tree Queue Disc Test Suite
 */
static class PifoTreeQueueDiscTestSuite : public TestSuite
{
public:
  PifoTreeQueueDiscTestSuite ()
    : TestSuite ("pifo-tree-queue-disc", UNIT)
  {
    AddTestCase (new PifoTreeQueueDiscTestCase (), TestCase::QUICK);
  }
} g_pifoTreeQueueTestSuite; ///< the test suite

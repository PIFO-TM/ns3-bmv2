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

#ifndef BASE_P4_PIPE_H
#define BASE_P4_PIPE_H

#define MAX_PKT_SIZE 2048
#define PACKET_LENGTH_REG_IDX 0

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/switch.h>

#include <memory>
#include <string>

#include "ns3/pointer.h"
#include "ns3/packet.h"

namespace ns3 {

/**
 * \ingroup p4-pipeline
 *
 * Base class for a P4 programmable pipeline.
 */
class BaseP4Pipe : public bm::Switch {
 public:
  /**
   * \brief Run the provided CLI commands to populate table entries
   */
  void run_cli(std::string commandsFile);

  /**
   * \brief Unused
   */
  int receive_(port_t port_num, const char *buffer, int len) override; 

  /**
   * \brief Unused
   */
  void start_and_return_() override; 

 protected:
  static bm::packet_id_t packet_id;
  static int thrift_port;
};

}

#endif /* P4_PIPELINE_H */


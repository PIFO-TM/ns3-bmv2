/*
Copyright 2018 ELTE Eötvös Loránd University
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Authors: 
	* Sandor Laki, lakis@elte.hu

*/

#include <core.p4>
//#include <v1model.p4>
#include "simple_pipe.p4"



/*
 * Proportional Integral Controller Enhanced (PIE) AQM is described in:
 *	Paper: https://ieeexplore.ieee.org/abstract/document/6602305
 *	RFC 8033: https://tools.ietf.org/html/rfc8033 
 *	Linux Kernel implementation: https://elixir.bootlin.com/linux/v4.13-rc7/source/net/sched/sch_pie.c
 *
 * Version 1: Fast path only implementation
 *
 *
*/

/* Typedefs */
typedef bit<32> queueDepth_t;
typedef bit<64> uint_t;

/* PIE parameters */
const uint_t cTimeUpdate = 30000000; // Update time in ns (30 ms); timer frequency
const uint_t cTarget = 20000000; // Target delay in ns (10 ms)
const uint_t cMaxProb =0xffffffff + 1; // 2^32 <=> (prob==1)
const queueDepth_t cLimit = 1000; // 1000 packets ; it could also be part of the buffer extern
const bool cECN = false;


/* Parameters of the PI controller */
const uint_t cAlpha = 125; //2; 
const uint_t cBeta = 1250; //20;

typedef bit<48>  EthernetAddress;

header Ethernet_h {
    EthernetAddress dstAddr;
    EthernetAddress srcAddr;
    bit<16>         etherType;
}

struct Parsed_packet {
    Ethernet_h    ethernet;
}

struct metadata_t {
}

parser parserI(packet_in pkt,
               out Parsed_packet hdr,
               inout metadata_t meta,
               inout standard_metadata_t stdmeta) {
    state start {
        pkt.extract(hdr.ethernet);
        transition accept;
    }
}

control DeparserI(packet_out packet,
                  in Parsed_packet hdr) {
    apply { packet.emit(hdr.ethernet); }
}

control cIngress(inout Parsed_packet hdr,
                 inout metadata_t meta,
                 inout standard_metadata_t stdmeta)
{

    bit<64> time_next;
    bit<64> now;
    uint_t qdelay_old;
    uint_t qdelay;
    uint_t prob;
    uint_t prob_old;
    int<64> delta;

    // states
    register<uint_t>(1) qdelay_reg;
    register<bit<64>>(1) time_next_reg;
    register<uint_t>(1) prob_reg;

    apply {
        /*
         * Ingress processing steps indepdent of the AQM go here.
         */


        if (stdmeta.qdepth>=cLimit) {
                stdmeta.drop = 1;
	}
        else {
                prob_reg.read(prob, 0);
                time_next_reg.read(time_next, 0);
                now = stdmeta.timestamp;
                qdelay = stdmeta.qlatency;

                if ( now >= time_next ) {
                   /* update probability */
                   qdelay_reg.read(qdelay_old, 0);
                   prob_reg.read(prob, 0);
                   delta = 0;

                   delta = (int<64>) ( cAlpha * (qdelay - cTarget) );
                   delta = delta + (int<64>) ( cBeta * (qdelay - qdelay_old) );

                   /* 
                     scaling factor for alpha ans beta are 1000 to avoid double point operations and we should map delta into the probability range of 0-2^32 -> 
                     -> delta = delta/1000/(10^9/2^32) -> approx.: delta / 2ˇ10 / (2^30/2^32) = delta / 2^8 = delta >> 8;
                   */
                   delta = delta >> 8;

                   /* We scale delta differently depending on whether we are in light, medium or high dropping mode. */
                   if (prob < cMaxProb/1000) {
                       delta = delta >> 5;
                   }
                   else if (prob < cMaxProb/100) {
                       delta = delta >> 3;
                   }
                   else if (prob < cMaxProb/10) {
                       delta = delta >> 1;
                   }
                   else {
                       delta = delta << 1;
                   }

                   /* increase probability in steps of no more than 2% */
                   if ((delta > (int<64>) (cMaxProb / (100 / 2))) && (prob >= cMaxProb / 10)) {
                        delta = (int<64>) (cMaxProb / (100 / 2)); /* set to 2% */
                   }

                   /* non-linear dropping */
                   if (qdelay > 250000000) { // 250 ms in ns
                       delta = delta + (int<64>)(cMaxProb / (100 / 2));
                   }

                   prob_old = prob;
                   prob = prob + (bit<64>)delta;

                   /* non-linear dropping  */
                   if ((qdelay == 0) && (qdelay_old == 0)) {
                       //prob = prob * 98 / 100; // cannot be evaluated at compile time
                       prob = prob - (prob >> 6); // approximation of the previous line: prob * 98.44 / 100;
                   }

                   /* Overflow check and handling */
                   if (delta>0) { 
                      if (prob < prob_old) {
                          prob = cMaxProb;
                      }
                   } else {
                      if (prob > prob_old) {
                          prob = 0;
                      }
                   }

                   /* update states */
                   prob_reg.write(0, prob);
                   qdelay_reg.write(0, qdelay);
                   time_next_reg.write(0, time_next + cTimeUpdate);
                }
                stdmeta.trace_var4 = (bit<32>)(prob);

                if (((qdelay >= cTarget/2) || (prob>=cMaxProb/5)) && (stdmeta.qdepth > 2) )
                {
                     bit<64> rand_val;
                     random<bit<64>>(rand_val, 0, cMaxProb);
                     if (rand_val < prob) {
                          if (cECN && (prob <= cMaxProb/10)) {
                               /* TODO: ECN mark instead of dropping */
                               stdmeta.drop = 1;
                          }
                          else {
                              stdmeta.drop = 1;
                          }
                     }
                }
                /* enqueue packet */
	}
    }

}

control cEgress(inout Parsed_packet hdr,
                inout metadata_t meta,
                inout standard_metadata_t stdmeta) {
    apply { }
}

control vc(inout Parsed_packet hdr,
           inout metadata_t meta) {
    apply { }
}

control uc(inout Parsed_packet hdr,
           inout metadata_t meta) {
    apply { }
}


V1Switch(parserI(),
    vc(),
    cIngress(),
    cEgress(),
    uc(),
    DeparserI()) main;


/* -*- P4_16 -*- */
#include <core.p4>
//#include <v1model.p4>
#include "simple_pipe.p4"

typedef bit<32> QueueDepth_t;
const bit<16> TYPE_IPV4 = 0x800;

// This value specifies size for table calc_red_drop_probability.
const bit<32> NUM_RED_DROP_VALUES = 1<<16; // 2^16
const bit<32> NUM_RED_DECAY_FACTORS = 1<<16; // 2^16

// RED parameters
// this should ideally be 9 but bmv2 limits bit shifts to 8
// qW = 2^(-log_qW) = 0.00390625
#define LOG_QW 8

/*************************************************************************
*********************** H E A D E R S  ***********************************
*************************************************************************/

struct metadata {
    /* empty */
}

struct headers {
    /* empty */
}

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/

parser MyParser(packet_in packet,
                out headers hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata) {

    state start {
        transition accept;
    }

}

/*************************************************************************
************   C H E C K S U M    V E R I F I C A T I O N   *************
*************************************************************************/

control MyVerifyChecksum(inout headers hdr, inout metadata meta) {   
    apply {  }
}


/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    register<QueueDepth_t>(1) avg_qdepth_reg;

    QueueDepth_t qdepth;
    QueueDepth_t avg_qdepth;
    bit<3> decay_factor; // bmv2 limits bit shift to 8 bits
    bit<9> drop_prob;
    bit<64> idle_duration;

    action set_decay_factor (bit<3> factor) {
        decay_factor = factor;
    }

    table calc_decay_factor {
        key = {
            idle_duration : range;
        }
        actions = {
            set_decay_factor;
        }
        // If there's a miss in the table the queue has been idle
        // for a long time and hence it is very likely empty
        const default_action = set_decay_factor(3); //7
        size = NUM_RED_DECAY_FACTORS;
    }

    action set_drop_probability (bit<9> drop_probability) {
        drop_prob = drop_probability;
    }

    table calc_red_drop_probability {
        key = {
            avg_qdepth : exact;
        }
        actions = {
            set_drop_probability;
        }
        const default_action = set_drop_probability(0);
        size = NUM_RED_DROP_VALUES;
    }
    
    apply {
        qdepth = standard_metadata.qdepth;

        // compute EWMA of queue depth
        @atomic {
          avg_qdepth_reg.read(avg_qdepth, 0);
          if (standard_metadata.qdepth != 0) {
              // compute: avg_qdepth = avg_qdepth + ((qdepth - avg_qdepth) >> LOG_QW);
              if (qdepth > avg_qdepth) {
                  avg_qdepth = avg_qdepth + ((qdepth - avg_qdepth) >> LOG_QW);
              }
              else {
                  avg_qdepth = avg_qdepth - ((avg_qdepth - qdepth) >> LOG_QW);
              }
          }
          else {
              idle_duration = standard_metadata.timestamp - standard_metadata.idle_time;
              calc_decay_factor.apply();
              avg_qdepth = avg_qdepth >> decay_factor;
          }
          avg_qdepth_reg.write(0, avg_qdepth);
        }
        // trace avg_qdepth
        standard_metadata.trace_var1 = avg_qdepth;

        calc_red_drop_probability.apply();
        bit<8> rand_val;
        random<bit<8>>(rand_val, 0, 255);
        // Of course someone might choose to do RED on multicast
        // packets in some way, too.  I am just avoiding the question
        // of what that might mean for this example.  One might also
        // wish to only apply RED for TCP packets, in which case the
        // 'if' condition would need to be changed to specify that.

        // drop_prob=0 means never do RED drop.  drop_prob in the
        // range [1, 256] means to drop a fraction (drop_prob/256) of
        // the time.  It is 9 bits in size so we can represent the
        // "always drop" case, desirable when the average queue depth
        // is high enough.
        if ( (bit<9>) rand_val < drop_prob) {
            standard_metadata.drop = 1;
        }
    }
}

/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyEgress(inout headers hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {
    apply {  }
}

/*************************************************************************
*************   C H E C K S U M    C O M P U T A T I O N   **************
*************************************************************************/

control MyComputeChecksum(inout headers  hdr, inout metadata meta) {
     apply { }
}

/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/

control MyDeparser(packet_out packet, in headers hdr) {
    apply { }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

V1Switch(
MyParser(),
MyVerifyChecksum(),
MyIngress(),
MyEgress(),
MyComputeChecksum(),
MyDeparser()
) main;

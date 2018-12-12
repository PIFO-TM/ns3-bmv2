/* -*- P4_16 -*- */
#include <core.p4>
#include "simple_pipe.p4"

/*
 * An implementation of the AFD AQM algorithm as described in
 * the following paper:
 *   - https://people.eecs.berkeley.edu/~istoica/classes/cs268/10/papers/afd.pdf
 */

/*
 * Include library that implements division. Set N and L params.
 */
#define N 32
#define L 10
#include "division.p4"

typedef bit<N> QueueDepth_t;
typedef int<N> SignedQueueDepth_t;
typedef bit<32> FlowID_t;
typedef bit<8> Prob_t;

//
// AFD Parameters
//

// See Guidelines 1, 2, and 3 in AFD paper
// increase NUM_SHADOW_ENTRIES to store more pkt samples
#define NUM_SHADOW_ENTRIES 512
#define L2_SHADOW_ENTRIES  9
// increase NUM_FLOW_ENTRIES to reduce hash collisions
#define NUM_FLOW_ENTRIES   4096
#define L2_FLOW_ENTRIES    12
// ALPHA and BETA are used to update fair_count
// alpha = 1.7 in paper
// ALPHA = log2(1.7/600000 * (2**31 - 1))
// ALPHA = log2(1.7)
#define ALPHA 1
// beta = 1.8 in paper
// BETA = log2(1.8/600000 * (2**31 - 1))
// BETA = log2(1.8)
#define BETA  2
typedef bit<L2_SHADOW_ENTRIES> ShadowIdx_t;
// QTARGET : the target queue size
//const QueueDepth_t QTARGET = 143165576*2; // equivalent to 80KB for N=31, max_qsize=600KB
//const QueueDepth_t QTARGET = 143165576; // equivalent to 40KB for N=31, max_qsize=600KB
//const QueueDepth_t QTARGET = 71582788; // equivalent to 20KB for N=31, max_qsize=600KB
//const QueueDepth_t QTARGET = 35791394; // equivalent to 10KB for N=31, max_qsize=600KB
const QueueDepth_t QTARGET = 8947848; // equivalent to 2.5KB for N=31, max_qsize=600KB
// SAMPLE_RATE : integer between 0 and 255.
// Dictates how often to include packet into shadow_buffer.
// See Guidelines 2 & 3 in the AFD paper
const Prob_t SAMPLE_RATE = 51; // sample about 1 in every 5 pkts

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

/*
 * Compute the fair_count.
 * This control block is executed at rate fs = 160Hz in the AFD paper.
 * So this control block should be executed every 1/(160Hz) = 6.25ms
 */
control compute_fair_count_pipe(in bit<1> timer_trigger,
                                in QueueDepth_t qdepth,
                                out QueueDepth_t fair_count) {

    QueueDepth_t old_fair_count;
    QueueDepth_t old_qdepth;

    // only used for debugging purposes
    table fair_count_debug {
        key = {
            old_fair_count : exact;
            fair_count : exact;
            old_qdepth : exact;
            qdepth : exact;
        }
        actions = { NoAction; }
        size = 1;
        default_action = NoAction;
    }

    register<QueueDepth_t>(1) fair_count_reg;
    register<QueueDepth_t>(1) qdepth_reg;

    apply {
        @atomic {
            fair_count_reg.read(fair_count, 0);
            if (timer_trigger == 1) {
                old_fair_count = fair_count;
                // executes every timer event
                qdepth_reg.read(old_qdepth, 0);
                // TODO: What happens when a negative number is bit shifted to the left? We want it to
                // keep it's sign and increase the magnitude.
                SignedQueueDepth_t qdiff = (SignedQueueDepth_t)(qdepth - QTARGET);
                SignedQueueDepth_t old_qdiff = (SignedQueueDepth_t)(old_qdepth - QTARGET);
                // The code below implements the following:
                // fair_count = old_fair_count + ((old_qdepth - QTARGET) * 2**ALPHA) - ((qdepth - QTARGET) * 2**BETA);
                if (old_qdiff < 0 && qdiff < 0) {
                    fair_count = (old_fair_count |-| ((QTARGET - old_qdepth) << ALPHA)) |+| ((QTARGET - qdepth) << BETA);
                }
                else if (old_qdiff < 0 && qdiff >= 0) {
                    fair_count = (old_fair_count |-| ((QTARGET - old_qdepth) << ALPHA)) |-| ((qdepth - QTARGET) << BETA);
                }
                else if (old_qdiff >= 0 && qdiff < 0) {
                    fair_count = (old_fair_count |+| ((old_qdepth - QTARGET) << ALPHA)) |+| ((QTARGET - qdepth) << BETA);
                }
                else { // both positive
                    fair_count = (old_fair_count |+| ((old_qdepth - QTARGET) << ALPHA)) |-| ((qdepth - QTARGET) << BETA);
                }
                fair_count_reg.write(0, fair_count);
                qdepth_reg.write(0, qdepth);
                fair_count_debug.apply();
            }
        }
    }
}

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {


    QueueDepth_t fair_count;
    QueueDepth_t flow_count;
    QueueDepth_t ratio;
    bit<32> flow_idx;
    Prob_t rand_val;
    bool insert_pkt;
    ShadowIdx_t rand_idx;
    QueueDepth_t size_to_add;
    QueueDepth_t size_to_remove;
    FlowID_t flow_to_add;
    FlowID_t flow_to_remove;
    Prob_t drop_prob;

    // only used for debugging purposes
    table shadow_debug {
        key = {
            rand_val : exact;
            insert_pkt : exact;
            rand_idx : exact;
            standard_metadata.pkt_len : exact;
            size_to_add : exact;
            size_to_remove : exact;
            flow_to_add : exact;
            flow_to_remove : exact;
        }
        actions = { NoAction; }
        size = 1;
        default_action = NoAction;
    }

    // only used for debugging purposes
    table flow_table_debug {
        key = {
            standard_metadata.flow_hash : exact;
            flow_idx : exact;
            flow_count : exact;
        }
        actions = { NoAction; }
        size = 1;
        default_action = NoAction;
    }

    // only used for debugging purposes
    table drop_debug {
        key = {
            flow_count : exact;
            fair_count : exact;
            ratio : exact;
            drop_prob : exact;
            rand_val : exact;
            standard_metadata.drop : exact;
        }
        actions = { NoAction; }
        size = 1;
        default_action = NoAction;
    }

    action set_drop_prob (Prob_t val) {
        drop_prob = val;
    }

    table calc_drop_prob {
        key = {
            ratio : exact;
        }
        actions = { set_drop_prob; }
        size = 256;
        // if flow_count/fair_count > 255 then drop packet
        default_action = set_drop_prob(255);
    }


    register<QueueDepth_t>(NUM_SHADOW_ENTRIES) shadow_buf_sizes;
    register<QueueDepth_t>(NUM_SHADOW_ENTRIES) shadow_buf_flowIDs;
    register<QueueDepth_t>(NUM_FLOW_ENTRIES) flow_table;

    compute_fair_count_pipe() compute_fair_count; 
    divide_pipe() divide; 

    apply {
        insert_pkt = false;
        size_to_add = standard_metadata.pkt_len;
        size_to_remove = 0;
        flow_to_add = standard_metadata.flow_hash;
        flow_to_remove = 0;
        QueueDepth_t qdepth = standard_metadata.qdepth;
        bit<1> timer_trigger = standard_metadata.timer_trigger;

        if (timer_trigger == 0) {
            // Insert packet into shadow_buffer w/ some probability
            random<Prob_t>(rand_val, 0, 255);
            if (rand_val < SAMPLE_RATE) {
                // Access the shadow_buffer
                insert_pkt = true;
                // Select a random packet to replace
                random<ShadowIdx_t>(rand_idx, 0, NUM_SHADOW_ENTRIES-1);
                @atomic {
                    shadow_buf_sizes.read(size_to_remove, (bit<32>)rand_idx);
                    shadow_buf_sizes.write((bit<32>)rand_idx, size_to_add);
                }
                @atomic {
                    shadow_buf_flowIDs.read(flow_to_remove, (bit<32>)rand_idx);
                    shadow_buf_flowIDs.write((bit<32>)rand_idx, flow_to_add);
                }
            }
            else {
                rand_idx = 0; // debugging only
            }
            shadow_debug.apply();

            /* Access the flow table to get flow_count (equivalent of m_i in the AFD paper)
             * Note that this requires 2 read & 2 write operations
             * to the flow_table
             */
            flow_idx = (bit<32>)flow_to_add[L2_FLOW_ENTRIES-1:0];
            @atomic {
                flow_table.read(flow_count, flow_idx);
                if (insert_pkt) {
                    // pkt was inserted into shadow buffer
                    flow_count = flow_count |+| size_to_add;
                    flow_table.write(flow_idx, flow_count);

                    // Decrement the count for the remove pkt
                    QueueDepth_t count;
                    bit<32> flow_dec_idx;
                    flow_dec_idx = (bit<32>)flow_to_remove[L2_FLOW_ENTRIES-1:0]; 
                    flow_table.read(count, flow_dec_idx);
                    count = count |-| size_to_remove;
                    flow_table.write(flow_dec_idx, count);
                }
            }
            flow_table_debug.apply();
        }
        else {
            flow_count = 0;
        }

        // Compute fair_count (equivalent of m_fair in AFD paper)
        compute_fair_count.apply(timer_trigger, qdepth, fair_count);

        // Compute the drop probability
        if (timer_trigger == 0) {
            divide.apply(flow_count, fair_count, ratio);
            calc_drop_prob.apply();
            random<Prob_t>(rand_val, 0, 255);
            if (rand_val < drop_prob) {
                standard_metadata.drop = 1;
            }
            drop_debug.apply();
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

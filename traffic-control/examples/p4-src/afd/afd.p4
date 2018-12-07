/* -*- P4_16 -*- */
#include <core.p4>
#include "simple_pipe.p4"
#include "division.p4"

/*
 * An implementation of the AFD AQM algorithm as described in
 * the following paper:
 *   - https://people.eecs.berkeley.edu/~istoica/classes/cs268/10/papers/afd.pdf
 */

typedef bit<32> QueueDepth_t;
typedef bit<32> FlowID_t;
typedef bit<8> Prob_t;

//
// AFD Parameters
//
//TODO set these appropriately

// See Guidelines 1, 2, and 3 in AFD paper
#define NUM_SHADOW_ENTRIES 1024
#define L2_SHADOW_ENTRIES  10
#define NUM_FLOW_ENTRIES   1024
#define L2_FLOW_ENTRIES    10
#define ALPHA 2
#define BETA  3
typedef bit<L2_SHADOW_ENTRIES> ShadowIdx_t;
// QTARGET : the target queue size
const QueueDepth_t QTARGET = 10000;
// SAMPLE_RATE : integer between 0 and 255.
// Dictates how often to include packet into shadow_buffer.
// See Guidelines 2 & 3 in the AFD paper
const Prob_t SAMPLE_RATE = 50;

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

    register<QueueDepth_t>(1) fair_count_reg;
    register<QueueDepth_t>(1) qdepth_reg;

    apply {
        QueueDepth_t old_fair_count;
        QueueDepth_t old_qdepth;

        @atomic {
            fair_count_reg.read(fair_count, 0);
            if (timer_trigger == 1) {
                old_fair_count = fair_count;
                // executes every timer event
                qdepth_reg.read(old_qdepth, 0);
                // TODO: What happens when a negative number is bit shifted to the right? We want it to
                // keep it's sign and increase the magnitude.
                fair_count = old_fair_count + ((old_qdepth - QTARGET) << ALPHA) - ((qdepth - QTARGET) << BETA);
                fair_count_reg.write(0, fair_count);
                qdepth_reg.write(0, qdepth);
            }
        }
    }
}

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    register<QueueDepth_t>(NUM_SHADOW_ENTRIES) shadow_buf_sizes;
    register<QueueDepth_t>(NUM_SHADOW_ENTRIES) shadow_buf_flowIDs;
    register<QueueDepth_t>(NUM_FLOW_ENTRIES) flow_table;

    compute_fair_count_pipe() compute_fair_count; 
    divide_pipe() divide; 

    apply {
        Prob_t rand_val;
        Prob_t keep_prob;
        bool insert_pkt = false;
        QueueDepth_t size_to_add = standard_metadata.pkt_len;
        QueueDepth_t size_to_remove = 0;
        FlowID_t flow_to_add = standard_metadata.flow_hash;
        FlowID_t flow_to_remove = 0;
        QueueDepth_t flow_count;
        QueueDepth_t qdepth = standard_metadata.qdepth;
        QueueDepth_t fair_count;
        bit<1> timer_trigger = standard_metadata.timer_trigger;

        if (timer_trigger == 0) {
            // Insert packet into shadow_buffer w/ some probability
            random<Prob_t>(rand_val, 0, 255);
            if (rand_val < SAMPLE_RATE) {
                // Access the shadow_buffer
                insert_pkt = true;
                ShadowIdx_t rand_idx;
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

            /* Access the flow table to get flow_count (equivalent of m_i in the AFD paper)
             * Note that this requires 2 read & 2 write operations
             * to the flow_table
             */
            bit<32> flow_idx = (bit<32>)flow_to_add[L2_FLOW_ENTRIES-1:0];
            @atomic {
                flow_table.read(flow_count, flow_idx);
                if (insert_pkt) {
                    // pkt was inserted into shadow buffer
                    flow_count = flow_count |+| size_to_add;
                    flow_table.write(flow_idx, flow_count);

                    // Decrement the count for the remove pkt
                    QueueDepth_t count;
                    flow_idx = (bit<32>)flow_to_remove[L2_FLOW_ENTRIES-1:0]; 
                    flow_table.read(count, flow_idx);
                    count = count |-| size_to_remove;
                    flow_table.write(flow_idx, count);
                }
            }
        }
        else {
            flow_count = 0;
        }

        // Compute fair_count (equivalent of m_fair in AFD paper)
        compute_fair_count.apply(timer_trigger, qdepth, fair_count);

        // Compute the drop probability
        if (timer_trigger == 0) {
            QueueDepth_t ratio;
            divide.apply(fair_count, flow_count, ratio);
            keep_prob = (Prob_t) ratio;
            random<Prob_t>(rand_val, 0, 255);
            if (rand_val > keep_prob) {
                standard_metadata.drop = 1;
            }
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

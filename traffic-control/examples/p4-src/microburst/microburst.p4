/* -*- P4_16 -*- */
#include <core.p4>
#include "simple_pipe.p4"

/*
 * Detect the number of flows using more than their share of buffer
 * space and who is currently using the most space.
 */

typedef bit<32> Uint_t;
typedef bit<32> QueueDepth_t;

// Define the fairshare of buffer space that each flow should not exceed
// If a flow exceeds this amount then it is counted as a culprit.
const QueueDepth_t QTHRESH = 10000; // 10KB of out 500KB

#define NUM_FLOW_ENTRIES 4096
#define L2_NUM_FLOW_ENTRIES 12

typedef bit<L2_NUM_FLOW_ENTRIES> FlowID_t;

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

control detect_microbust_pipe(in bit<1> ingress_trigger,
                              in FlowID_t ingress_flowID,
                              in bit<1> enq_trigger,
                              in QueueDepth_t enq_pkt_len_bytes,
                              in FlowID_t enq_flowID,
                              in bit<1> deq_trigger,
                              in QueueDepth_t deq_pkt_len_bytes,
                              in FlowID_t deq_flowID,
                              out Uint_t num_culprits,
                              out QueueDepth_t flow_count) {

    QueueDepth_t enq_count;
    QueueDepth_t deq_count;
    bool inc_culprits;
    bool dec_culprits;

    // only used for debugging purposes
    table microburst_debug {
        key = {
            ingress_trigger : exact;
            ingress_flowID : exact;
            enq_trigger : exact;
            enq_flowID : exact;
            enq_pkt_len_bytes : exact;
            enq_count : exact;
            deq_trigger : exact;
            deq_flowID : exact;
            deq_pkt_len_bytes : exact;
            deq_count : exact;
            num_culprits : exact;
            flow_count : exact;
        }
        actions = { NoAction; }
        size = 1;
        default_action = NoAction;
    }

    register<QueueDepth_t>(NUM_FLOW_ENTRIES) flow_count_reg;
    register<Uint_t>(1) num_culprits_reg;

    apply {
        // NOTE: flow_count_reg must be at least a dual port register
        // because it requires 2 read and 2 writes to potentially
        // different indicies: increment @ enq_flowID,
        // decrement @ deq_flowID
        // If the architecture supports a triple port register then
        // we could use the third port to query the flow_count of
        // ingress packets and use that information to make drop
        // decisions (e.g. FRED).
        enq_count = 0;
        deq_count = 0;
        flow_count = 0;
        @atomic {
            // NOTE: if any flowIDs are the same then we can reduce the
            // number of state accesses
            if (enq_trigger==1) {
                // increment byte count for enq_flowID
                flow_count_reg.read(enq_count, (bit<32>)enq_flowID);
                enq_count = enq_count |+| enq_pkt_len_bytes;
                flow_count_reg.write((bit<32>)enq_flowID, enq_count);
            }

            if (deq_trigger==1) {
                // decrement byte count for deq_flowID
                flow_count_reg.read(deq_count, (bit<32>)deq_flowID);
                deq_count = deq_count |-| deq_pkt_len_bytes;
                flow_count_reg.write((bit<32>)deq_flowID, deq_count);
            }

            if (ingress_trigger==1) {
                // query the flow_count of the ingress packet
                flow_count_reg.read(flow_count, (bit<32>)ingress_flowID);
            }
        }

        // default: don't change num_culprits
        inc_culprits = false;
        dec_culprits = false;

        // determine if we should change num_culprits
        if (enq_trigger==1 && deq_trigger==1 && enq_flowID == deq_flowID) {
            // special case where pkts from the same flow are enqueued and dequeued at the same time
            if (deq_count > QTHRESH && (deq_count - enq_pkt_len_bytes + deq_pkt_len_bytes) <= QTHRESH) {
                inc_culprits = true;
            }
            else if (deq_count <= QTHRESH && (deq_count - enq_pkt_len_bytes + deq_pkt_len_bytes) > QTHRESH) {
                dec_culprits = true;
            }
        }
        else {
            // typical case where enq and deq pkts are from different flows
            if (enq_trigger==1 && enq_count > QTHRESH && (enq_count - enq_pkt_len_bytes) <= QTHRESH) {
                inc_culprits = true;
            }
            if (deq_trigger==1 && deq_count <= QTHRESH && (deq_count + deq_pkt_len_bytes) > QTHRESH) {
                dec_culprits = true;
            }
        }

        // update num_culprits
        @atomic {
            num_culprits_reg.read(num_culprits, 0);
            if (inc_culprits) {
                num_culprits = num_culprits |+| 1;
            }
            if (dec_culprits) {
                num_culprits = num_culprits |-| 1;
            }
            num_culprits_reg.write(0, num_culprits);
        }

        microburst_debug.apply();

        // I was initially going to try and maintain two state variables
        // (main_culprit and main_culprit_count) which would keep track of
        // the flow with the largest count in the flow_count_table. However,
        // I realized that this is not really possible. Imagine a scenario
        // where the current main_culprit dequeues a bunch of packets and
        // hence it's count goes way down. At that point, we would have no
        // idea which flow has the max count until that flow happens to
        // enqueue or dequeue a packet (which would probably happen pretty
        // quickly in practice). So something like the code below might
        // behave reasonably well in real scenarios.

        // // determine if a byte count was increased and if so, the final count value
        // if (enq_trigger==1 && deq_trigger==1 && (enq_flowID == deq_flowID) && (enq_pkt_len_bytes > deq_pkt_len_bytes)) {
        //     count_increased = true;
        //     culprit_count = deq_count;
        // }
        // else if (enq_trigger==1 && deq_trigger==0) {
        //     count_increased = true;
        //     culprit_count = enq_count;
        // }
        // else {
        //     count_increased = false;
        //     culprit_count = 0; // unused
        // }
        // // update the main_culprit and main_culprit_count
        // // NOTE: the main_culprit_reg is not gauranteed to always contain the flowID
        // // of the flow with the largest count. Imagine a scenario where the 
        // @atomic {
        //     main_culprit_count_reg.read(main_culprit_count, 0);
        //     main_culprit_reg.read(main_culprit, 0);
        //     // adjust main_culprit_count if the main_culprit flow dequeued a packet
        //     if (deq_trigger==1 && deq_flowID == main_culprit) {
        //         main_culprit_count = main_culprit_count - deq_pkt_len_bytes;
        //     }
        //     // if a flow count was increased then we may need to update the main_culprit
        //     if (count_increased) {
        //         if (enq_flowID == main_culprit) {
        //             main_culprit_count = culprit_count;
        //         }
        //         else if (culprit_count > main_culprit_count) {
        //             main_culprit_count = culprit_count;
        //             main_culprit = enq_flowID;
        //         }
        //     }
        //     main_culprit_count_reg.write(0, main_culprit_count);
        //     main_culprit_reg.write(0, main_culprit);
        // }
    }
}

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    QueueDepth_t flow_count;
    Uint_t num_culprits;
    FlowID_t flowID;
    detect_microbust_pipe() detect_microburst;
    
    apply {
        flowID = standard_metadata.flow_hash[L2_NUM_FLOW_ENTRIES-1:0];
        detect_microburst.apply(standard_metadata.ingress_trigger,
                                flowID,
                                standard_metadata.enq_trigger,
                                standard_metadata.enq_pkt_len_bytes,
                                standard_metadata.enq_flow_hash[L2_NUM_FLOW_ENTRIES-1:0],
                                standard_metadata.deq_trigger,
                                standard_metadata.deq_pkt_len_bytes,
                                standard_metadata.deq_flow_hash[L2_NUM_FLOW_ENTRIES-1:0],
                                num_culprits,
                                flow_count);
        standard_metadata.trace_var1 = num_culprits;
        standard_metadata.trace_var2 = (bit<32>)flowID;
        standard_metadata.trace_var3 = flow_count;
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

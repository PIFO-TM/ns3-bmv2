/* -*- P4_16 -*- */
#include <core.p4>
#include "enq_pipe.p4"
#include "dummy_blocks.p4"

/* Windowed Strict Priority:
 * Over each window of length T, at most N bytes from flow i are served
 * if there are any lower priority packets waiting to be served.
 */

#define NUM_FLOWS 3
#define NUM_PRIORITIES 3
#define MAX_PKTS 3

control EnqueueLogic(inout headers hdr,
                     inout metadata meta,
                     inout standard_metadata_t standard_metadata) {

    bit<32> flow_id;
    bit<32> deq_windowID;
    bit<32> cur_windowID;
    bit<32> last_windowID;
    bit<32> pkt_count;
    register<bit<32>>(1) cur_window_reg;
    register<bit<32>>(NUM_FLOWS) last_window_reg;
    register<bit<32>>(NUM_FLOWS) pkt_count_reg;

    table enq_debug {
        key = {
            standard_metadata.enq_trigger : exact;
            standard_metadata.pkt_len : exact;
            standard_metadata.flow_hash : exact;
            standard_metadata.buffer_id : exact;
            standard_metadata.partition_id : exact;
            standard_metadata.partition_size : exact;
            standard_metadata.partition_max_size : exact;
            standard_metadata.timestamp : exact;
            standard_metadata.is_leaf : exact;
            standard_metadata.child_node_id : exact;
            standard_metadata.child_pifo_id : exact;
            standard_metadata.deq_trigger : exact;
            standard_metadata.deq_rank : exact;
            standard_metadata.deq_tx_time : exact;
            standard_metadata.deq_tx_delta : exact;
            standard_metadata.deq_user_meta : exact; // new
            standard_metadata.deq_pkt_len : exact;
            standard_metadata.deq_flow_hash : exact;
            standard_metadata.deq_buffer_id : exact;
            standard_metadata.deq_partition_id : exact;
            standard_metadata.deq_partition_size : exact;
            standard_metadata.deq_partition_max_size : exact;
        }
        actions = {
            NoAction;
        }
        const default_action = NoAction;
        size = 1;
    }

    /*
     * Approach:
     * Incomming flows can assign at most N packets to
     * each window, the dequeue events advance the service window.
     * Each packet contains a rank and the ID of the window to which
     * it belongs. The window ID is passed back in dequeue events
     * to advance the service window. Flows can only assign packets
     * to either the current window or a future window. This is to
     * avoid situations where, for example, flow A doesn't send any
     * packets for a long time and then sends a long burst which all
     * receive very high priority.
     * The problem with this policy is that if it is used at a parent
     * node then it wouldn't be able to limit the number of bytes
     * assigned to each window, because a PIFO entry can point to
     * different packets after subsequent enqueue operations.
     */

    apply {
        enq_debug.apply();

        flow_id = standard_metadata.buffer_id;
        deq_windowID = standard_metadata.deq_user_meta;

        // update cur_window_reg
        @atomic {
            cur_window_reg.read(cur_windowID, 0);
            if (standard_metadata.deq_trigger == 1) {
                cur_windowID = deq_windowID;
            }
            cur_window_reg.write(0, cur_windowID);
        }

        // Track the number of packets assigned to each window
        /*
         * Two state variables here:
         * last_window_reg - track the ID of the window that the last packet from this flow was assigned to
         * pkt_count_reg - track the number of packets from this flow assigned to last_windowID
         */
        @atomic {
            // update last_windowID if it is in the past or if we've 
            last_window_reg.read(last_windowID, flow_id);
            pkt_count_reg.read(pkt_count, flow_id);
            if (standard_metadata.enq_trigger == 1) {
                if (last_windowID >= cur_windowID) {
                    pkt_count = pkt_count |+| 1;
                    // update last_windowID if we've exceeded the limit for this window
                    if (pkt_count > MAX_PKTS) {
                        last_windowID = last_windowID |+| NUM_PRIORITIES;
                    }
                }
                else {
                    pkt_count = 1;
                    // update last_windowID if it is in the past
                    last_windowID = cur_windowID;
                }
            }
            pkt_count_reg.write(flow_id, pkt_count);
            last_window_reg.write(flow_id, last_windowID);
        }

        // set the outputs
        standard_metadata.rank = last_windowID |+| flow_id;
        standard_metadata.user_meta = last_windowID;
        standard_metadata.pifo_id = 0;

    }
}

V1Switch(
DummyParser(),
DummyVerifyChecksum(),
EnqueueLogic(),
DummyEgress(),
DummyComputeChecksum(),
DummyDeparser()
) main;


/*
 * Alternate approaches:
 *
 * Approach 1: token bucket for high priority packets, if
 * there are not enough tokens then the packets are assigned
 * low priority. If there are enough tokens then the packets
 * are assigned high priority.  
 *
 * Approach 2: decaying counter for high priority packets,
 * once a certain limit is exceeded, use low priority for
 * these packets.
 *
 * Approach 3: Use per-flow token buckets to limit the dequeue
 * rate of each flow. Assign lower priority to flows that exceed
 * dequeue rate limit. What does "lower priority" mean here?
 *
 */


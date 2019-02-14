/* -*- P4_16 -*- */
#include <core.p4>
#include "enq_pipe.p4"
#include "dummy_blocks.p4"

/* Windowed Strict Priority:
 * Over each window of length T, at most N bytes from flow i are served
 * if there are any lower priority packets waiting to be served.
 *
 * NOTE: this implementation doesn't work because regardless of the rate
 * at which high priority packets are being sent, every MAX_PKTS packets
 * their priority will be decreased.
 */

control EnqueueLogic(inout headers hdr,
                     inout metadata meta,
                     inout standard_metadata_t standard_metadata) {

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

#define NUM_FLOWS 3
#define NUM_PRIORITIES 3
#define MAX_PKTS 3

    bool inc_rank;
    bit<32> rank;
    bit<32> flow_id;
    bit<32> pkt_count;
    register<bit<32>>(NUM_FLOWS) pkt_count_reg;
    register<bit<32>>(NUM_FLOWS) last_rank_reg;

    apply {
        enq_debug.apply();

        flow_id = standard_metadata.buffer_id;
        inc_rank = false; // default

        // update pkt_count_reg
        @atomic {
            pkt_count_reg.read(pkt_count, flow_id);
            if (standard_metadata.enq_trigger == 1) {
                pkt_count = pkt_count + 1;
                if (pkt_count > MAX_PKTS) {
                    inc_rank = true;
                    pkt_count = 0;
                }
            }
            pkt_count_reg.write(flow_id, pkt_count);
        }

        // update last_rank_reg and max_rank_reg
        @atomic {
            max_rank_reg.read(max_rank, 0);
            last_rank_reg.read(rank, flow_id);
            if (inc_rank) {
                rank = rank + flow_id + max_rank;
                max_rank = rank;
            }
            last_rank_reg.write(flow_id, rank);
            max_rank.write(0, max_rank);
        }

        // set the outputs
        standard_metadata.rank = rank;
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

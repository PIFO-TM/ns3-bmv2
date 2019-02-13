/* -*- P4_16 -*- */
#include <core.p4>
#include "enq_pipe.p4"
#include "dummy_blocks.p4"

#define MAX_NUM_FLOWS 32

typedef bit<32> rank_t;

control EnqueueLogic(inout headers hdr,
                     inout metadata meta,
                     inout standard_metadata_t standard_metadata) {

    // declare virtual_time & last_finish reg arrays
    register<rank_t>(1) virtual_time_reg;
    register<rank_t>(MAX_NUM_FLOWS) last_finish_reg;

    rank_t virtual_time;
    rank_t last_finish;
    bit<32> flow_id;
    rank_t start_time;

    table enq_inputs_debug {
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

    table enq_outputs_debug {
        key = {
            standard_metadata.rank : exact;
            standard_metadata.pifo_id : exact;
        }
        actions = {
            NoAction;
        }
        const default_action = NoAction;
        size = 1;
    }

    apply {
        enq_inputs_debug.apply();

        // read / update virtual_time state
        @atomic {
            virtual_time_reg.read(virtual_time, 0);
            if (standard_metadata.deq_trigger == 1) {
                virtual_time = standard_metadata.deq_rank;
                virtual_time_reg.write(0, virtual_time);
            }
        }

        // each flow is assigned a different buffer ID in classification
        flow_id = standard_metadata.buffer_id;

        // update last_finish state
        if (standard_metadata.enq_trigger == 1) {
            @atomic {
                last_finish_reg.read(last_finish, flow_id);
                if (last_finish > virtual_time) {
                    start_time = last_finish;
                }
                else {
                    start_time = virtual_time;
                }
                last_finish = start_time + standard_metadata.pkt_len;
                last_finish_reg.write(flow_id, last_finish);
            }
            // set outputs
            standard_metadata.rank = start_time;
            standard_metadata.pifo_id = 0;
            enq_outputs_debug.apply();
        }

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

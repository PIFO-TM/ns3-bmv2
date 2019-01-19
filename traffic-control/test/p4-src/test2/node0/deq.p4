/* -*- P4_16 -*- */
#include <core.p4>
#include "deq_pipe.p4"
#include "dummy_blocks.p4"

control DequeueLogic(inout headers hdr,
                     inout metadata meta,
                     inout standard_metadata_t standard_metadata) {

    table deq_debug {
        key = {
            standard_metadata.timestamp : exact;
            standard_metadata.is_leaf : exact;
            standard_metadata.pifo0_is_empty : exact;
            standard_metadata.pifo0_last_deq_time : exact;
            standard_metadata.pifo0_child_node_id : exact;
            standard_metadata.pifo0_child_pifo_id : exact;
            standard_metadata.pifo0_rank : exact;
            standard_metadata.pifo0_tx_time : exact;
            standard_metadata.pifo0_tx_delta : exact;
            standard_metadata.pifo0_pkt_len : exact;
        }
        actions = {
            NoAction;
        }
        const default_action = NoAction;
        size = 1;
    }
    
    apply {
        deq_debug.apply();
        standard_metadata.pifo_id = 0;
    }
}

V1Switch(
DummyParser(),
DummyVerifyChecksum(),
DequeueLogic(),
DummyEgress(),
DummyComputeChecksum(),
DummyDeparser()
) main;

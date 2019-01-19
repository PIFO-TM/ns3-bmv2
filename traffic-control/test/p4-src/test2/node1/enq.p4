/* -*- P4_16 -*- */
#include <core.p4>
#include "enq_pipe.p4"
#include "dummy_blocks.p4"

control EnqueueLogic(inout headers hdr,
                     inout metadata meta,
                     inout standard_metadata_t standard_metadata) {

    table enq_debug {
        key = {
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
        }
        actions = {
            NoAction;
        }
        const default_action = NoAction;
        size = 1;
    }
    
    apply {
        enq_debug.apply();
        standard_metadata.rank = 10;
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

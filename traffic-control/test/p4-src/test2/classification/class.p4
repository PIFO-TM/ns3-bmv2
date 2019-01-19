/* -*- P4_16 -*- */
#include <core.p4>
#include "classification_pipe.p4"
#include "dummy_blocks.p4"

control ClassificationLogic(inout headers hdr,
                            inout metadata meta,
                            inout standard_metadata_t standard_metadata) {

    table classification_debug {
        key = {
            standard_metadata.pkt_len : exact;
            standard_metadata.flow_hash : exact;
            standard_metadata.timestamp : exact;
        }
        actions = {
            NoAction;
        }
        const default_action = NoAction;
        size = 1;
    }
    
    apply {
        classification_debug.apply();
        standard_metadata.buffer_id = 0;
        standard_metadata.leaf_id = 1;
    }
}

V1Switch(
DummyParser(),
DummyVerifyChecksum(),
ClassificationLogic(),
DummyEgress(),
DummyComputeChecksum(),
DummyDeparser()
) main;

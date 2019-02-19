/* -*- P4_16 -*- */
#include <core.p4>
#include "classification_pipe.p4"
#include "dummy_blocks.p4"

#define HASH_TABLE_SIZE 1024
#define L2_HASH_TABLE_SIZE 10

typedef bit<32> index_t;
typedef bit<32> uint_t;

control ClassificationLogic(inout headers hdr,
                            inout metadata meta,
                            inout standard_metadata_t standard_metadata) {

    index_t index;
    bit<1> is_active;
    uint_t num_flows;
    uint_t buffer_id;
    register<bit<1>>(HASH_TABLE_SIZE) active_flows_reg;
    register<uint_t>(1) num_flows_reg;
    register<uint_t>(HASH_TABLE_SIZE) buffer_map_reg;

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

        // Check if this flow is already active
        index = (index_t)(standard_metadata.flow_hash[L2_HASH_TABLE_SIZE-1:0]);
        @atomic {
            active_flows_reg.read(is_active, index);
            active_flows_reg.write(index, 1);
        }

        // update num_flows register
        @atomic {
            num_flows_reg.read(num_flows, 0);
            if (is_active == 0) {
                num_flows = num_flows + 1;
            }
            num_flows_reg.write(0, num_flows);
        }

        // compute buffer_id for this flow
        @atomic {
            buffer_map_reg.read(buffer_id, index);
            if (is_active == 0) {
                // first packet of this flow
                buffer_id = num_flows |-| 1;
            }
            buffer_map_reg.write(index, buffer_id);
        }

        // set the classification results
        standard_metadata.buffer_id = buffer_id;
        standard_metadata.leaf_id = 0;
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

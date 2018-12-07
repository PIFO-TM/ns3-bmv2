/* -*- P4_16 -*- */
#include <core.p4>
#include "simple_pipe.p4"

/*
 * Implements a simple token bucket using the periodic time
 * reference feature.
 */

typedef bit<32> uint_t;

// time reference = 1ms
// FILL_RATE = 125 bytes/ms = 1Mbps
const uint_t FILL_RATE = 125; // bytes / slot
const uint_t MAX_TOKENS = 1000; // two 500B packets

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

control token_bucket(in bit<1> timer_trigger, // set deterministically every PERIOD
                     in uint_t request, // number of requested tokens
                     out bool result)
{
    // externs
    register<uint_t>(1) tokens_reg;

    // metadata
    uint_t tokens;

    apply {
        @atomic {
            tokens_reg.read(tokens, 0);
            if (timer_trigger == 1) {
                // timer event (deterministically every PERIOD)
                tokens = tokens + FILL_RATE;
                if (tokens > MAX_TOKENS) {
                    tokens = MAX_TOKENS;
                }
                result = true;
            }
            else {
                // legit packet arrival
                if (tokens > request) {
                    result = true;
                    tokens = tokens - request;
                }
                else {
                    result = false;
                }
            }
            tokens_reg.write(0, tokens);
        }
    }
}

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    token_bucket() tb; 

    apply {
        uint_t request = standard_metadata.pkt_len_bytes;
        bool result;

        tb.apply(standard_metadata.timer_trigger, request, result);

        if (result == false) {
            standard_metadata.drop = 1;
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

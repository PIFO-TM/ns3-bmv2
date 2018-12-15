/* -*- P4_16 -*- */
#include <core.p4>
#include "simple_pipe.p4"

/*
 * Simple P4 program to compute instantaneous queue size using
 * dequeue events.
 */

typedef bit<32> QueueDepth_t;

const QueueDepth_t QTHRESH = 15000; // 15KB

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

control compute_qsize_pipe(in bit<1> enq_trigger,
                           in QueueDepth_t enq_pkt_len_bytes,
                           in bit<1> deq_trigger,
                           in QueueDepth_t deq_pkt_len_bytes,
                           out QueueDepth_t qsize) {

    // only used for debugging purposes
    table qsize_debug {
        key = {
            enq_trigger : exact;
            enq_pkt_len_bytes : exact;
            deq_trigger : exact;
            deq_pkt_len_bytes : exact;
            qsize : exact;
        }
        actions = { NoAction; }
        size = 1;
        default_action = NoAction;
    }

    register<QueueDepth_t>(1) qsize_reg;

    apply {
        @atomic {
            qsize_reg.read(qsize, 0);
            if (enq_trigger==1 && deq_trigger==0) {
                qsize = qsize |+| enq_pkt_len_bytes;
            }
            else if (enq_trigger==0 && deq_trigger==1) {
                qsize = qsize |-| deq_pkt_len_bytes;
            }
            else if (enq_trigger==1 && deq_trigger==1) {
                // NOTE: this case will never be executed because of the
                // way dequeue events are implemented in ns3-bmv2
                qsize = qsize |+| enq_pkt_len_bytes |-| deq_pkt_len_bytes;
            }
            qsize_reg.write(0, qsize);
        }
        qsize_debug.apply();
    }
}

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    QueueDepth_t qsize;
    compute_qsize_pipe() compute_qsize;
    
    apply {
        compute_qsize.apply(standard_metadata.enq_trigger,
                            standard_metadata.enq_pkt_len_bytes,
                            standard_metadata.deq_trigger,
                            standard_metadata.deq_pkt_len_bytes,
                            qsize);
        standard_metadata.trace_var1 = qsize;
        if (qsize > QTHRESH) {
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

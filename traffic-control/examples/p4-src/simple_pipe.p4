/*
Copyright 2013-present Barefoot Networks, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/* P4-16 declaration of the P4 simple pipe */

#ifndef _SIMPLE_PIPE_P4_
#define _SIMPLE_PIPE_P4_

#include "core.p4"

match_kind {
    range,
    // Used for implementing dynamic_action_selection
    selector
}

@metadata @name("standard_metadata")
struct standard_metadata_t {
    //
    // Inputs
    //
    /* qdepth:
     * The instantaneous size of the queue. Note that this is
     * not measured in bytes. Here is the equation that converts
     * qdepthBytes to qdepth:
     *     qdepth = qdepthBytes/maxSizeBytes * (2^qsizeBits - 1)
     * Where maxSizeBytes is the maximum size of the queue in bytes,
     * and qsizeBits is the number of bits that are used to represent
     * size (both queue and packet). qsizeBits is a configurable
     * parameter for the p4-queue-disc module in ns3-bmv2.
     */
    bit<32> qdepth;
    /* avg_qdepth:
     * The EWMA of the queue size. Computed using the same technique
     * as the RED queue disc implementation. Again, note that this is
     * not measured in bytes, but rather is mapped into an integer in
     * the range [0, 2^qsizeBits - 1]. See the description of the qdepth
     * field for an equation to convert bytes to an integer in this range.
     */
    bit<32> avg_qdepth;
    /* timestamp:
     * The time that the packet arrived, measured in nanoseconds.
     */
    bit<64> timestamp;
    /* idle_time:
     * The time when the queue last went empty, measured in nanoseconds.
     */
    bit<64> idle_time;
    /* qlatency:
     * The latest queue latency measurement. Determined by computing the
     * difference between the enqueue and dequeue timestamps for each
     * packet. Measured in nanoseconds.
     */
    bit<64> qlatency;
    /* pkt_len:
     * The length of the packet. Note that this is not measured in bytes.
     * The packet length is transformed into an integer in the range
     * [0, 2^qsizeBits - 1] so that it is directly comparable to the 
     * qdepth and avg_qdepth fields.
     */
    bit<32> pkt_len;
    /* l3_proto:
     * The L3 protocol number (IPv4, IPv6, etc.)
     */
    bit<16> l3_proto;
    /* flow_hash:
     * A hash of identifying packet fields, e.g. the 5-tuple. Can be used
     * to identify flows.
     */
    bit<32> flow_hash;
    //
    // Outputs
    //
    /* drop:
     * If set then p4-queue-disc will drop the packet.
     */
    bit<1>  drop;
    /* mark:
     * If set then p4-queue-disc will mark the packet (e.g. set ECN bit).
     */
    bit<1>  mark;
    //
    // Inputs / Outputs
    //
    /* tarce_vars:
     * These are intended to be used for debugging purposes. The p4-queue-disc
     * has attached trace sources to these fields so that NS3 user scripts
     * can attach trace sinks to them can hence can track all changes made to
     * them. 
     */
    bit<32> trace_var1;
    bit<32> trace_var2;
    bit<32> trace_var3;
    bit<32> trace_var4;

    /// Error produced by parsing
    error parser_error;
}

enum CounterType {
    packets,
    bytes,
    packets_and_bytes
}

enum MeterType {
    packets,
    bytes
}

extern counter {
    counter(bit<32> size, CounterType type);
    void count(in bit<32> index);
}

extern direct_counter {
    direct_counter(CounterType type);
    void count();
}

extern meter {
    meter(bit<32> size, MeterType type);
    void execute_meter<T>(in bit<32> index, out T result);
}

extern direct_meter<T> {
    direct_meter(MeterType type);
    void read(out T result);
}

extern register<T> {
    register(bit<32> size);
    void read(out T result, in bit<32> index);
    void write(in bit<32> index, in T value);
}

// used as table implementation attribute
extern action_profile {
    action_profile(bit<32> size);
}

// Get a random number in the range lo..hi
extern void random<T>(out T result, in T lo, in T hi);
// If the type T is a named struct, the name is used
// to generate the control-plane API.
extern void digest<T>(in bit<32> receiver, in T data);

enum HashAlgorithm {
    crc32,
    crc32_custom,
    crc16,
    crc16_custom,
    random,
    identity,
    csum16,
    xor16
}

extern void mark_to_drop();
extern void hash<O, T, D, M>(out O result, in HashAlgorithm algo, in T base, in D data, in M max);

extern action_selector {
    action_selector(HashAlgorithm algorithm, bit<32> size, bit<32> outputWidth);
}

enum CloneType {
    I2E,
    E2E
}

@deprecated("Please use verify_checksum/update_checksum instead.")
extern Checksum16 {
    Checksum16();
    bit<16> get<D>(in D data);
}

/**
Verifies the checksum of the supplied data.
If this method detects that a checksum of the data is not correct it
sets the standard_metadata checksum_error bit.
@param T          Must be a tuple type where all the fields are bit-fields or varbits.
                  The total dynamic length of the fields is a multiple of the output size.
@param O          Checksum type; must be bit<X> type.
@param condition  If 'false' the verification always succeeds.
@param data       Data whose checksum is verified.
@param checksum   Expected checksum of the data; note that is must be a left-value.
@param algo       Algorithm to use for checksum (not all algorithms may be supported).
                  Must be a compile-time constant.
*/
extern void verify_checksum<T, O>(in bool condition, in T data, inout O checksum, HashAlgorithm algo);
/**
Computes the checksum of the supplied data.
@param T          Must be a tuple type where all the fields are bit-fields or varbits.
                  The total dynamic length of the fields is a multiple of the output size.
@param O          Output type; must be bit<X> type.
@param condition  If 'false' the checksum is not changed
@param data       Data whose checksum is computed.
@param checksum   Checksum of the data.
@param algo       Algorithm to use for checksum (not all algorithms may be supported).
                  Must be a compile-time constant.
*/
extern void update_checksum<T, O>(in bool condition, in T data, inout O checksum, HashAlgorithm algo);

/**
Verifies the checksum of the supplied data including the payload.
The payload is defined as "all bytes of the packet which were not parsed by the parser".
If this method detects that a checksum of the data is not correct it
sets the standard_metadata checksum_error bit.
@param T          Must be a tuple type where all the fields are bit-fields or varbits.
                  The total dynamic length of the fields is a multiple of the output size.
@param O          Checksum type; must be bit<X> type.
@param condition  If 'false' the verification always succeeds.
@param data       Data whose checksum is verified.
@param checksum   Expected checksum of the data; note that is must be a left-value.
@param algo       Algorithm to use for checksum (not all algorithms may be supported).
                  Must be a compile-time constant.
*/
extern void verify_checksum_with_payload<T, O>(in bool condition, in T data, inout O checksum, HashAlgorithm algo);
/**
Computes the checksum of the supplied data including the payload.
The payload is defined as "all bytes of the packet which were not parsed by the parser".
@param T          Must be a tuple type where all the fields are bit-fields or varbits.
                  The total dynamic length of the fields is a multiple of the output size.
@param O          Output type; must be bit<X> type.
@param condition  If 'false' the checksum is not changed
@param data       Data whose checksum is computed.
@param checksum   Checksum of the data.
@param algo       Algorithm to use for checksum (not all algorithms may be supported).
                  Must be a compile-time constant.
*/
extern void update_checksum_with_payload<T, O>(in bool condition, in T data, inout O checksum, HashAlgorithm algo);

extern void resubmit<T>(in T data);
extern void recirculate<T>(in T data);
extern void clone(in CloneType type, in bit<32> session);
extern void clone3<T>(in CloneType type, in bit<32> session, in T data);

extern void truncate(in bit<32> length);

// The name 'standard_metadata' is reserved

// Architecture.
// M should be a struct of structs
// H should be a struct of headers or stacks

parser Parser<H, M>(packet_in b,
                    out H parsedHdr,
                    inout M meta,
                    inout standard_metadata_t standard_metadata);

/* The only legal statements in the implementation of the
VerifyChecksum control are: block statements, calls to the
verify_checksum and verify_checksum_with_payload methods,
and return statements. */
control VerifyChecksum<H, M>(inout H hdr,
                             inout M meta);
@pipeline
control Ingress<H, M>(inout H hdr,
                      inout M meta,
                      inout standard_metadata_t standard_metadata);
@pipeline
control Egress<H, M>(inout H hdr,
                     inout M meta,
                     inout standard_metadata_t standard_metadata);

/* The only legal statements in the implementation of the
ComputeChecksum control are: block statements, calls to the
update_checksum and update_checksum_with_payload methods,
and return statements. */
control ComputeChecksum<H, M>(inout H hdr,
                              inout M meta);
@deparser
control Deparser<H>(packet_out b, in H hdr);

package V1Switch<H, M>(Parser<H, M> p,
                       VerifyChecksum<H, M> vr,
                       Ingress<H, M> ig,
                       Egress<H, M> eg,
                       ComputeChecksum<H, M> ck,
                       Deparser<H> dep
                       );

#endif  /* _SIMPLE_PIPE_P4_ */

#!/usr/bin/env python

import numpy as np
import sys, os, argparse

"""
This script is used to generate entries for two tables:
(1) calc_red_drop_probability[exact] - maps avg queue size [0, 2^size_bits - 1]
    into drop probability [0, 256]
(2) calc_decay_factor[range] - maps queue idle duration [0, max_dur*1e9 ns]
    into decay factor [0, 32]
"""

commandsFile = "commands.txt"
dropDataFile = "drop_probability.plotme"
drop_entry_fmat = "table_add calc_red_drop_probability set_drop_probability {} => {}\n"
decay_entry_fmat = "table_add calc_decay_factor set_decay_factor {}->{} => {} {}\n"

avg_qsizes = []
drop_vals = []

def gen_decay_commands(num_entries, max_dur, pkt_size, link_rate, qW):
    assert(qW < 1)
    s = pkt_size*8.0/link_rate
    # compute the logarithmically spaced points we'd like to evaluate the decay function at
    idle_durs = np.logspace(0, np.log10(max_dur + 0.9)/np.log10(s), num_entries, base=s) - 0.9
    # evaluate decay function at selected points
    decay_factors = (1 - qW)**(idle_durs/s)
    log_decay_factors = np.log2(decay_factors)
    with open(commandsFile, 'a') as f:
        for dur, log_decay, prio in zip(idle_durs, log_decay_factors, range(len(idle_durs))):
            # find closest negative power of 2
            approx_log = -int(round(log_decay))
            approx_log = 7 if (approx_log > 7) else approx_log # to deal with bmv2 bit shift limit
            range_min  = 0
            range_max = int(round(dur*1e9)) # convert to ns
            f.write(decay_entry_fmat.format(range_min, range_max, approx_log, prio))

def gen_drop_commands(bits, minTh, maxTh, slope, offset):
    with open(commandsFile, 'a') as f:
        for qsize in range(1<<bits):
            if qsize < minTh:
                drop_prob = 0
            elif qsize > maxTh:
                drop_prob = 256
            else:
                drop_prob = int(slope*qsize + offset)
                drop_prob = 256 if (drop_prob > 256) else drop_prob
            f.write(drop_entry_fmat.format(qsize, drop_prob))
            avg_qsizes.append(qsize)
            drop_vals.append(drop_prob)

def map_to_int(size, max_size, bits):
    return int(round( (float(size)/max_size) * ((1<<bits) - 1) ))

def map_to_byte(size, max_size, bits):
    return (float(size)/(1<<bits - 1)) *  max_size

def write_data(max_size, size_bits):
    with open(dropDataFile, 'w') as f:
        for x, y in zip(avg_qsizes, drop_vals):
            x = map_to_byte(x, max_size, size_bits)
            f.write('{} {}\n'.format(x, y))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-s','--size_bits', type=int, help='<Required> The number of bits used to represent queue size', default=13)
    parser.add_argument('-m','--max_size', type=int, help='<Required> The maximum possible queue size (B)', default=500000)
    parser.add_argument('-l','--low', type=int, help='<Required> The RED min threshold (B)', default=5000)
    parser.add_argument('-u','--upper', type=int, help='<Required> The RED max threshold (B)', default=15000)
    parser.add_argument('-t','--max_dur', type=float, help='<Required> The maximum idle_duration to use (sec)', default=3.0)
    parser.add_argument('-n','--num_decay_entries', type=int, help='<Required> The number of entries to use in the decay factor table', default=10)
    parser.add_argument('-p','--pkt_size', type=int, help='<Required> Packet size to use to compute s (B)', default=1000)
    parser.add_argument('-r','--link_rate', type=int, help='<Required> Link rate to use to compute s (bit/sec)', default=1500000)
    parser.add_argument('-q','--queue_weight', type=float, help='<Required> The weight given to new queue size samples (must be the same as in P4 program)', default=2**-8)
    parser.add_argument('-w', '--write', action='store_true', default=False, help="Write the calculated drop probabilities to a file")
    args = parser.parse_args()

    # clear commands file
    with open(commandsFile, 'w') as f:
        f.write('')

    #
    # Compute decay factor table entries
    #
    gen_decay_commands(args.num_decay_entries, args.max_dur, args.pkt_size, args.link_rate, args.queue_weight)

    #
    # Compute drop probability table entries
    #
    minTh = map_to_int(args.low, args.max_size, args.size_bits)
    maxTh = map_to_int(args.upper, args.max_size, args.size_bits)

    th_diff = float(maxTh - minTh)
    slope = 1.0 / th_diff
    offset = -minTh / th_diff

    print 'minTh = {}'.format(minTh)
    print 'maxTh = {}'.format(maxTh)
    print 'slope = {}'.format(slope)
    print 'offset = {}'.format(offset)

    gen_drop_commands(args.size_bits, minTh, maxTh, slope, offset)
    if args.write:
        write_data(args.max_size, args.size_bits)

if __name__ == "__main__":
    main()


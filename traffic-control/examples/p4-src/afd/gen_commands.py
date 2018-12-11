#!/usr/bin/env python

import numpy as np
import sys, os, argparse
import p4_division as div

"""
This script is used to generate entries for the division tables used in the AFD prototype:
(1) log_numerator[ternary] - maps uint in [0,2^N-1] to logarithm in [0,2^l-1] using logarithmically
    spaced entries
(2) log_denominator[ternary] - maps uint in [0,2^N-1] to logarithm in [0,2^l-1] using logarithmically
    spaced entries
(3) exp[exact] - maps logarithm in [0,2^l-1] back to uint in [0,2^N-1]

This script is also used to initialize the fair_count register based on the workload:
fair_count = NUM_SHADOW_ENTRIES * (fair_share_rate/ingress_link_rate)
"""

commandsFile = "commands.txt"
drop_prob_fmat = "table_add calc_drop_prob set_drop_prob {} => {}\n"
log_num_fmat = "table_add log_numerator set_log_num 0b{:0%db}&&&0b{:0%db} => {} {}\n"
log_denom_fmat = "table_add log_denominator set_log_denom 0b{:0%db}&&&0b{:0%db} => {} {}\n"
exp_fmat = "table_add exp set_result {} => {}\n"
reg_write_fmat = "register_write fair_count_reg 0 {}\n"
reg_read_fmat = "register_read fair_count_reg 0\n"

def gen_drop_probs(max_rand):
    with open(commandsFile, 'a') as f:
        for ratio in range(256):
            if ratio == 0:
                drop_prob = 0
            else:
                drop_prob = int(round((1.0 - 1.0/float(ratio)) * max_rand))
            f.write(drop_prob_fmat.format(ratio, drop_prob));

def gen_division_commands(N, l, m):
    global commandsFile
    global log_num_fmat
    global log_denom_fmat
    global exp_fmat 
    div.N = N
    div.l = l
    div.m = m
    div.make_tables()
    log_num_fmat = log_num_fmat % (N, N)
    log_denom_fmat = log_denom_fmat % (N, N)
    with open(commandsFile, 'a') as f:
        # write log table entries
        for (prio, data, mask, val) in div.log_table:
            f.write(log_num_fmat.format(data, mask, val, prio))
        for (prio, data, mask, val) in div.log_table:
            f.write(log_denom_fmat.format(data, mask, val, prio))
        # write exp table entries
        for (key, val) in div.exp_table:
            f.write(exp_fmat.format(key, val))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-r','--max_rand', type=int, help='<Required> The maximum value in the random number range', default=255)
    parser.add_argument('-N','--uint_bits', type=int, help='<Required> The number of bits used to represent queue size', default=31)
    parser.add_argument('-l','--log_bits', type=int, help='<Required> The number of bits to use to represent logarithms', default=10)
    parser.add_argument('-m','--log_accuracy', type=int, help='<Required> The window of calculation accurary', default=6)
    parser.add_argument('-b', '--shadow_buf_pkts', type=int, help='<Required> The number of packets stored in tge shadow buffer (equivalent to b variable in the AFD paper)', default=512)
    parser.add_argument('-f', '--fairshare', type=float, help='<Required> The initial fairshare rate', default=4.0)
    parser.add_argument('-R', '--ingress_rate', type=float, help='<Required> The total ingress rate (must be same units as fairshare)', default=14.0)
    parser.add_argument('-M', '--max_buf_size', type=float, help='<Required> The maximum possible queue size (B)', default=600000.0)
    parser.add_argument('-p', '--pkt_size', type=float, help='<Required> The pkt size (B)', default=1000.0)
    args = parser.parse_args()

    # clear commands file
    with open(commandsFile, 'w') as f:
        f.write('')

    #
    # Initialize the fair_count_reg
    #
    fair_count = (args.shadow_buf_pkts * args.fairshare/args.ingress_rate) * args.pkt_size
    fair_count = int(round( fair_count/args.max_buf_size * (2**args.uint_bits - 1) ))
#    with open(commandsFile, 'a') as f:
#        f.write(reg_write_fmat.format(fair_count))
#        f.write(reg_read_fmat)
    print 'initial fair_count = {}'.format(float(fair_count)/(2**args.uint_bits - 1) * args.max_buf_size)

    #
    # Initialize the calc_drop_prob_table 
    #
    gen_drop_probs(args.max_rand)

    #
    # Compute division table entries 
    #
    gen_division_commands(args.uint_bits, args.log_bits, args.log_accuracy)

if __name__ == "__main__":
    main()


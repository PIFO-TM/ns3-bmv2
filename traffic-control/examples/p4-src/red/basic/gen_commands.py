#!/usr/bin/env python

import sys, os, argparse

"""
This script is used to generate table entries that map
avg queue size [0, 2^bits - 1] into drop probability [0, 256]
"""

commandsFile = "commands.txt"
dataFile = "drop_probability.plotme"
entry_fmat = "table_add calc_red_drop_probability set_drop_probability {} => {}\n"

xdata = []
ydata = []

def gen_commands(bits, minTh, maxTh, slope, offset):
    with open(commandsFile, 'w') as f:
        for qsize in range(1<<bits):
            if qsize < minTh:
                drop_prob = 0
            elif qsize > maxTh:
                drop_prob = 256
            else:
                drop_prob = int(slope*qsize + offset)
                drop_prob = 256 if (drop_prob > 256) else drop_prob
            f.write(entry_fmat.format(qsize, drop_prob))
            xdata.append(qsize)
            ydata.append(drop_prob)

def map_to_int(size, max_size, bits):
    return int(round( (float(size)/max_size) * ((1<<bits) - 1) ))

def map_to_byte(size, max_size, bits):
    return (float(size)/(1<<bits - 1)) *  max_size

def write_data(max_size, bits):
    with open(dataFile, 'w') as f:
        for x, y in zip(xdata, ydata):
            x = map_to_byte(x, max_size, bits)
            f.write('{} {}\n'.format(x, y))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-b','--bits', type=int, help='<Required> The number of bits used to represent queue size', required=True)
    parser.add_argument('-m','--max_size', type=int, help='<Required> The maximum possible queue size (B)', required=True)
    parser.add_argument('-l','--low', type=int, help='<Required> The RED min threshold (B)', required=True)
    parser.add_argument('-u','--upper', type=int, help='<Required> The RED max threshold (B)', required=True)
    parser.add_argument('-w', '--write', action='store_true', default=False, help="Write the calculated drop probabilities to a file")
    args = parser.parse_args()

    minTh = map_to_int(args.low, args.max_size, args.bits)
    maxTh = map_to_int(args.upper, args.max_size, args.bits)

    th_diff = float(maxTh - minTh)
    slope = 1.0 / th_diff
    offset = -minTh / th_diff

    print 'minTh = {}'.format(minTh)
    print 'maxTh = {}'.format(maxTh)
    print 'slope = {}'.format(slope)
    print 'offset = {}'.format(offset)

    gen_commands(args.bits, minTh, maxTh, slope, offset)
    if args.write:
        write_data(args.max_size, args.bits)

if __name__ == "__main__":
    main()


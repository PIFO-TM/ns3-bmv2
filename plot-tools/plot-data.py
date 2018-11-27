#!/usr/bin/env python

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import sys, os, argparse

def line_gen():
    lines = ['-', '--', ':', '-.']
    i = 0
    while True:
        yield lines[i]
        i += 1
        i = i % len(lines)

line_generator = line_gen()

def color_gen():
    colors = ['b', 'g', 'r', 'c']
    i = 0
    while True:
        yield colors[i]
        i += 1
        i = i % len(colors)

color_generator = color_gen()

def read_data(data_file):
    xdata = []
    ydata = []
    with open(data_file) as f:
        for line in f:
            l = map(float, line.strip().split())
            if len(l) != 2:
                print >> sys.stderr, "ERROR: unexpected line in data file: {}".format(line)
                sys.exit(1)
            xdata.append(l[0])
            ydata.append(l[1])
    return xdata, ydata

def plot_data(data_file, label):
    xdata, ydata = read_data(data_file)
    plt.plot(xdata, ydata, label=label, linestyle=line_generator.next(), color=color_generator.next(), linewidth=3)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d','--data', nargs='+', help='<Required> The data files to plot', required=True)
    parser.add_argument('-l','--labels', nargs='+', help='<Required> The plot labels to use', required=True)
    parser.add_argument('-x','--xlabel', help='The xlabel to use', required=False)
    parser.add_argument('-y','--ylabel', help='The ylabel to use', required=False)
    parser.add_argument('-t','--title', help='The plot title to use', required=False)
    parser.add_argument('-k','--hlines', type=float, nargs='+', help='Horizontal lines to plot', required=False, default=[])
    args = parser.parse_args()

    if len(args.data) != len(args.labels):
        print >> sys.stderr, "ERROR: The number of data files ({}) and labels ({}) does not agree".format(len(args.data), len(args.labels))
        sys.exit(1)

    fig = plt.figure()
    plt.title(args.title)
    plt.xlabel(args.xlabel)
    plt.ylabel(args.ylabel)
    for data, label in zip(args.data, args.labels):
        plot_data(data, label)
    for y in args.hlines:
        plt.axhline(y=y, color='r', linestyle=':', linewidth=3)
    plt.legend()
    plt.show()

if __name__ == "__main__":
    main()


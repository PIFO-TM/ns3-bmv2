#!/usr/bin/env python

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import sys, os, argparse

def main():
    fig = plt.figure()
    plt.title('Decay Function')
    plt.xlabel('Idle Duration')
    plt.ylabel('Decay Factor')

    # NOTE: For a 500KB queue that is drained at 1.5Mbps, it takes
    # about 2.7 seconds to be completely drained

    max_duration = 2.7 # sec

    avg_pkt_size = 200.0 # bytes
    link_rate = 1.5e6 # bits/sec
    qW = 2**-9 # queue weight

    s = avg_pkt_size*8.0/link_rate # typical pkt transmission time

    #idle_duration = np.linspace(0, max_duration, 1000)

    base = s
    idle_duration = np.logspace(0, np.log10(max_duration + 0.9)/np.log10(base), 20, base=base) - 0.9
    decay_factor = (1 - qW)**(idle_duration/s)

    plt.plot(idle_duration, decay_factor, marker='o', linewidth=3, markersize=10)
    plt.plot(idle_duration, idle_duration*0, marker='x', markersize=13, color='r')

    plt.show()

if __name__ == "__main__":
    main()


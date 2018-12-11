#!/usr/bin/env python

"""
This module implements integer division using the fact that:
A/B = exp(log(A) - log(B))

Involves:
  - 2 ternary match table lookups to approximate log(A) and log(B)
  - integer subtraction to compute: diff = log(A) - log(B)
  - 1 exact match table lookup to approximate: exp(diff)

API - 2 functions:
  1. make_tables() - sets up the log_table and exp_table
  2. divide(a,b) - performs the approximate integer division
"""

import sys, os
from math import exp, log
import numpy as np

"""
N=16, m=6, l=9 gives these results:
len(log_table) =  383
total_mem = 1885.75 bytes
"""

N = 32
m = 6 
l = 10

log_table = []
exp_table = []

def gen_log_keys():
    keys = []
    fmat_key = '{:0%db}' % N
    for key in range(1,2**m):
        key_str = fmat_key.format(key)
        keys.append(key_str)

    fmat_sub_key = '{:0%db}' % m
    for i in range(1, N-m+1):
        for k in range(2**(m-1), 2**m):
            key_str = '0'*(N-m-i) + fmat_sub_key.format(k) + 'X'*i
            keys.append(key_str)
    return keys

"""
Input
-----
keys: list of strings of the form '010XX'

Generates entries of form: (addr, data, mask, val)

NOTE: Assumes the X's are all at the end
"""
def gen_log_entries(keys):
    addr = 0
    for key in keys:
        wc_len = key.count('X')
        mask_len = len(key) - wc_len
        mask = (2**mask_len -1) << wc_len
        data = int(key.replace('X','0'),2)
        avg = find_avg(key)
        val = f_log(avg)
        log_table.append((addr, data, mask, val))
        addr += 1

"""
Find the average value covered by the data & mask

i.e. 011X => (6+7)/2 = 6.5
"""
def find_avg(key):
    min_val = int(key.replace('X','0'),2)
    max_val = int(key.replace('X','1'),2)
    return (min_val + max_val)/2.0

def f_log(x):
    return int(round(log(x)/log(2**N-1)*(2**l-1)))

"""
Finds the appropriate match in the log_table for input integer x
"""
def apply_log_table(x):
    for (addr, data, mask, val) in log_table:
        if data == (x & mask):
            return val
    print >> sys.stderr, "ERROR: could not find match in log_table for input {}".format(x)
    sys.exit(1)

def print_log_table():
    print "log_table:"
    print "----------"
    for (data, mask), val in log_table.items():   
        fmat = "({:0%db}, {:0%db}) ==> {}" % (N, N)
        print fmat.format(data, mask, val)
    print "len(log_table) = ", len(log_table)

"""
Generates the entries for the exp_table.

Exact match table mapping [0, 2^l-1] => [0, 2^N-1]
"""
def gen_exp_entries():
    for key in range(2**l):
        exp_table.append((key, f_log_inv(key)))

def f_log_inv(x):
    return int(round(exp((x/(2**l-1.0))*log(2**N-1))))

def apply_exp_table_slow(x):
    for (key, val) in exp_table:
        if key == x:
            return val
    print >> sys.stderr, "ERROR: could not find match in exp_table for input {}".format(x)
    sys.exit(1)

def apply_exp_table(x):
    lo, hi = 0, len(exp_table) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if exp_table[mid][0] < x:
            lo = mid + 1
        elif x < exp_table[mid][0]:
            hi = mid - 1
        else:
            return exp_table[mid][1]
    print >> sys.stderr, "ERROR: could not find match in exp_table for input {}".format(x)
    sys.exit(1)

def print_exp_table():
    print "exp_table:"
    print "----------"
    for (key, val) in exp_table:
        print "{} ==> {}".format(key, val)
    print "len(exp_table) = ", len(exp_table)

def check_size(x):
    try:
        assert(len(bin(x))-2 <= N)
    except:
        print >> sys.stderr, "ERROR: {} cannot be represented in {} bits".format(x, N)
        sys.exit(1)

def make_tables():
    keys = gen_log_keys()
    gen_log_entries(keys)
    gen_exp_entries()
    print "len(log_table) = ", len(log_table)
    print "len(exp_table) = ", len(exp_table)

def divide(a,b):
    check_size(a)
    check_size(b)

    if (a == 0 or b == 0 or b > a):
        return 0

    log_a = apply_log_table(a)
    log_b = apply_log_table(b)
    return apply_exp_table(log_a-log_b)



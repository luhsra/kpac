#!/usr/bin/env python3

import subprocess as sp
import numpy as np
import argparse
import csv
import os

BENCHMARKS = 'benchmarks.txt'
NTIMES = 1000
DEST = '.'

def run(path, n):
    desc = os.sep.join(path.split(os.sep)[-2:])
    diff = np.zeros(n)

    for i in range(n):
        print(f'\r{desc}: {i+1}/{n}', end='')
        out = sp.check_output(["./do_bench", path])
        diff[i] = float(out)

    mean = diff.mean()
    std = diff.std()
    masked = np.ma.masked_outside(diff, mean-2*std, mean+2*std)
    masked_count = np.ma.count_masked(masked)
    print(": %.9f (%.9f); %d masked" % (mean, std, masked_count))

    return masked

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Run PAC-SW benchmarks.')
    parser.add_argument('dest', metavar = 'OUTPUT', nargs = '?', default = DEST, type = str,
                        help = f'destination directory for results (default: {DEST})')
    parser.add_argument('-n', metavar = 'NTIMES', default = NTIMES, type = int,
                        help = f'amount of samples to collect (default: {NTIMES})')
    parser.add_argument('-i', metavar = 'INPUT', default = BENCHMARKS, type = str,
                        help = f'source of benchmarks (default: {BENCHMARKS})')

    args = parser.parse_args()

    with open(args.i) as f:
        benchmarks = f.read().splitlines()

    pac = {}
    bare = {}

    # Perform measurements
    for b in benchmarks:
        if b.startswith('#'):
            continue
        b = os.path.normpath(b)
        name = b.split(os.sep)[-1]

        bare[name] = run(os.sep.join([b, 'bare']), args.n)
        pac[name] = run(os.sep.join([b, 'pac']), args.n)

    FIELDS = ['Benchmark', 'Mean', 'SD', 'Min', 'Max']

    # Write the results
    os.makedirs(args.dest, exist_ok=True)

    with open(os.path.join(args.dest, 'bare.csv'), 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k, v in bare.items():
            w.writerow([k, v.mean(), v.std(), v.min(), v.max()])

    with open(os.path.join(args.dest, 'pac.csv'), 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k, v in pac.items():
            w.writerow([k, v.mean(), v.std(), v.min(), v.max()])

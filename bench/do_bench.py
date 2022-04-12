#!/usr/bin/env python3

import subprocess as sp
import numpy as np
import csv

def run(path, n=10):
    desc = '/'.join(path.split('/')[-2:])
    diff = np.zeros(n)

    for i in range(n):
        print(f'\r{desc}: {i+1}/{n}', end='')
        out = sp.check_output(["./do_bench", path])
        diff[i] = float(out)

    print(": %.9f (%.9f)" % (diff.mean(), diff.std()))

    return diff

if __name__ == '__main__':
    with open('benchmarks.txt') as f:
        benchmarks = f.read().splitlines()

    pac = {}
    bare = {}

    for b in benchmarks:
        if b.startswith('#'):
            continue
        name = b.split('/')[-1]

        bare[name] = run(b + '/bare')
        pac[name] = run(b + '/pac')

    FIELDS = ['Benchmark', 'Mean', 'SD', 'Min', 'Max']

    with open('results/bare.csv', 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k, v in bare.items():
            w.writerow([k, v.mean(), v.std(), v.min(), v.max()])

    with open('results/pac.csv', 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k, v in pac.items():
            w.writerow([k, v.mean(), v.std(), v.min(), v.max()])

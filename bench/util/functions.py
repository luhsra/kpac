#!/usr/bin/env python3

import subprocess as sp
import argparse
import os, sys
import csv, re
import signal

BENCHMARKS = 'benchmarks.txt'

def run(path):
    desc = os.sep.join(path.split(os.sep)[-2:])

    pac_sw = sp.Popen(["../sw/pac-sw"], stdout=sp.PIPE);
    bench = sp.run([path])
    if bench.returncode != 0:
        pac_sw.terminate()
        raise RuntimeError("Benchmark finished with nonzero return code.")

    pac_sw.send_signal(signal.SIGINT)
    pac_sw.wait()

    outp = str(pac_sw.stdout.read())
    pac_sw.stdout.close()

    pac = re.findall('pac: (\d+)', outp)[0]
    aut = re.findall('aut: (\d+)', outp)[0]

    return (pac, aut)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Count pointer authorizations performed by benchmarks.')
    parser.add_argument('-i', metavar = 'INPUT', default = BENCHMARKS, type = str,
                        help = f'source of benchmarks (default: {BENCHMARKS})')

    args = parser.parse_args()

    with open(args.i) as f:
        benchmarks = f.read().splitlines()

    w = csv.writer(sys.stdout)
    w.writerow(['name', 'pac', 'aut'])

    for b in benchmarks:
        if b.startswith('#'):
            continue
        b = os.path.normpath(b)

        (pac, aut) = run(os.sep.join([b, 'pac']))
        name = b.split(os.sep)[-1]

        w.writerow([name, pac, aut])

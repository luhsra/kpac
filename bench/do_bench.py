#!/usr/bin/env python3

import subprocess as sp
import numpy as np
import argparse

import os
import sys
import tempfile
import signal

import re

import csv
import toml

# Plugin artefacts
PLUGIN_DIR	= os.path.abspath('../gcc')
PLUGIN_DLL	= os.path.join(PLUGIN_DIR, 'pac_sw_plugin.so')

KPACD_DIR	= "/sys/kernel/debug/kpacd"
KPACD_NR_PAC	= os.path.join(KPACD_DIR, "nr_pac");
KPACD_NR_AUT	= os.path.join(KPACD_DIR, "nr_aut");

# Plugin flags for PAC builds
PAC_ARGS_INST = { 'sign-scope': 'std' }

verbose = False

def get_attr(attr):
    val = 0
    with open(attr) as f:
        val = int(f.read())
    return val

class Benchmark:
    def __init__(self, name, j):
        self.name = name

        # Values which are not provided in the benchmark section are retrieved
        # from the toplevel
        b = j['benchmarks'][name]
        self.path = b['path']
        self.warmup = b.get('warmup', j.get('warmup', 0))
        self.samples = b.get('samples', j['samples'])
        self.do_run = b.get('do_run', j['do_run'])
        self.do_build = b.get('do_build', j['do_build'])

    # Run a command in the benchmark directory
    def do(self, cmd):
        if not verbose:
            cmd += ' > /dev/null'
        argv = ['sh', '-xc' if verbose else '-c', cmd]
        sp.check_call(argv, cwd=self.path)

    def build(self):
        self.do(self.do_build)

    # Measure average run duration of the benchmark
    def meas(self):
        nr_pac_0 = get_attr(KPACD_NR_PAC)
        nr_aut_0 = get_attr(KPACD_NR_AUT)
        diff = np.zeros(self.samples)

        cmd = self.do_run
        if not verbose:
            cmd += ' > /dev/null'

        # Make sure to restore cwd when done
        cwd = os.getcwd()
        os.chdir(self.path)

        for i in range(-self.warmup, self.samples):
            sys.stdout.write(f'\r{self.name}: {i+1}/{self.samples:<12d}\r')
            sys.stdout.flush()
            res = bench.run(['/bin/sh', '-c', cmd])
            if i >= 0:
                diff[i] = res

        os.chdir(cwd)

        # Filter out values outside of 95th percentile
        mean = diff.mean()
        std = diff.std()
        rstd = std/mean*100
        masked = np.ma.masked_outside(diff, mean-2*std, mean+2*std)
        masked_count = np.ma.count_masked(masked)
        sys.stdout.write(f'{self.name}: {mean:#.9f} ({rstd:#.3f} %); '
                         f'{masked_count} outliers rejected\n')

        nr_pac = get_attr(KPACD_NR_PAC) - nr_pac_0
        nr_aut = get_attr(KPACD_NR_AUT) - nr_aut_0

        if nr_pac != nr_aut:
            # Something weird is up
            raise RuntimeError(f'PAC and AUT do not match ({nr_pac} != {nr_aut}).')

        return masked, nr_pac//(self.warmup+self.samples)

class Suite:
    def __init__(self, path):
        with open(path) as f:
            j = toml.load(f)

        self.benchmarks = []
        for b in j['benchmarks'].keys():
            self.benchmarks.append(Benchmark(b, j))

    # Build the benchmarks with specified plugin arguments, compile
    # instrumentation statistics
    def build(self, args=None):
        stat = {}

        with tempfile.NamedTemporaryFile(mode='r+') as tmp:
            if args:
                args['inst-dump'] = tmp.name
                os.environ['PAC_FLAGS'] = plugin_args(PLUGIN_DLL, args)
            else:
                os.environ['PAC_FLAGS'] = ""

            for b in self.benchmarks:
                stat[b.name] = [0, 0]
                tmp.truncate(0)

                b.build()

                # Compile stats of this benchmark
                tmp.seek(0)
                for row in csv.reader(tmp):
                    stat[b.name][0] += int(row[1])
                    stat[b.name][1] += int(row[2])

        return stat

    def meas(self):
        results = {}
        auths = {}
        for b in self.benchmarks:
            results[b.name], auths[b.name] = b.meas()

        return results, auths

# Construct gcc flags for plugin and its arguments
def plugin_args(plugin, args):
    name = ''.join(os.path.basename(plugin).split('.')[:-1])

    s = '-fplugin=' + plugin
    for k, v in args.items():
        s += ' -fplugin-arg-' + name + '-' + k + '=' + v

    return s

# Dump processed statistics
def dump_stat(f, data):
    FIELDS = ['name', 'mean', 'std', 'min', 'max']
    with open(f, 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k, v in data.items():
            w.writerow([k, v.mean(), v.std(), v.min(), v.max()])

# Dump raw measurements
def dump_raw(f, data):
    with open(f, 'w') as f:
        w = csv.writer(f)
        v = data.values()
        maxlen = max([len(i) for i in v])
        w.writerow(data.keys())
        for i in range(maxlen):
            w.writerow([j[i] if i < len(j) else None for j in v])

# Dump build instrumentation facts
def dump_inst(f, inst, auths):
    FIELDS = ['name', 'inst', 'total', 'auths']
    with open(f, 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k in inst.keys():
            w.writerow([k, inst[k][0], inst[k][1], auths[k]])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compiler and run PAC-SW benchmarks.')
    parser.add_argument('suite', metavar='SUITE', nargs=1, type=str,
                        help='input file with benchmark suite description')
    parser.add_argument('dest', metavar='OUTPUT', nargs=1, type=str,
                        help=f'destination directory for results')
    parser.add_argument('-r', action='store_true', help='retain raw measurement data')
    parser.add_argument('-v', action='store_true', help='display commands when executing')
    args = parser.parse_args()

    verbose = args.v
    dest = args.dest[0]

    # Compile benchmarking utility module
    sp.check_call(['make', '-C', '.'])
    import bench

    # Recompile plugin if needed
    sp.check_call(['make', '-C', PLUGIN_DIR])

    # Make sure output directory exists
    os.makedirs(dest, exist_ok=True)

    suite = Suite(args.suite[0])

    # Pure run without PAC
    suite.build()
    data, _ = suite.meas()

    dump_stat(os.path.join(dest, 'nopac.csv'), data)
    if args.r:
        dump_raw(os.path.join(dest, 'nopac_raw.csv'), data)

    # PAC-instrumented run
    inst = suite.build(PAC_ARGS_INST)
    data, auths = suite.meas()

    dump_stat(os.path.join(dest, 'pac.csv'), data)
    if args.r:
        dump_raw(os.path.join(dest, 'pac_raw.csv'), data)

    dump_inst(os.path.join(dest, 'build.csv'), inst, auths)

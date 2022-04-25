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

DEST = '.'

PAC_SW_DIR = os.path.abspath('../sw')
PAC_SW_BIN = os.path.join(PAC_SW_DIR, 'pac-sw')

# Plugin artefacts
PLUGIN_DIR	= os.path.abspath('../gcc')
PLUGIN_DLL	= os.path.join(PLUGIN_DIR, 'pac_sw_plugin.so')
PAC_PROLOGUE	= os.path.join(PLUGIN_DIR, 'prologue.s')
PAC_EPILOGUE    = os.path.join(PLUGIN_DIR, 'epilogue.s')
PAC_OBJ		= os.path.join(PLUGIN_DIR, 'map_device.o')

# Plugin flags for non-PAC builds
PAC_ARGS_BASE = { 'init-func': 'map_device' } # Include the init function for
                                              # fair measurements

# Plugin flags for PAC builds
PAC_ARGS_INST = { **PAC_ARGS_BASE,
                  'prologue': PAC_PROLOGUE,
                  'epilogue': PAC_EPILOGUE }

verbose = False

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

        return masked

    # Retrieve the amount of authentications performed in single run
    def auths(self):
        pac_sw = sp.Popen([PAC_SW_BIN], stdout = sp.PIPE);
        try:
            print(f'Counting {self.name} authentications...')
            self.do(self.do_run)
        except:
            # Don't leave daemons running in the background
            pac_sw.terminate()
            raise

        pac_sw.send_signal(signal.SIGINT)
        pac_sw.wait()

        outp = str(pac_sw.stdout.read())
        pac_sw.stdout.close()

        pac = re.findall('pac: (\d+)', outp)[0]
        aut = re.findall('aut: (\d+)', outp)[0]

        if pac != aut:
            # Something weird is up
            raise RuntimeError(f'PAC and AUT do not match ({pac} != {aut}).')

        return pac

class Suite:
    def __init__(self, path):
        with open(path) as f:
            j = toml.load(f)

        self.benchmarks = []
        for b in j['benchmarks'].keys():
            self.benchmarks.append(Benchmark(b, j))

    # Build the benchmarks with specified plugin arguments, compile
    # instrumentation statistics if needed
    def build(self, args={}, dump=False):
        stat = {}

        with tempfile.NamedTemporaryFile(mode='r+') as tmp:
            if dump:
                args['inst-dump'] = tmp.name

            # Set relevant environment variables
            os.environ['PAC_OBJ'] = PAC_OBJ
            os.environ['PAC_FLAGS'] = plugin_args(PLUGIN_DLL, args)

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
        for b in self.benchmarks:
            results[b.name] = b.meas()

        return results

    def auths(self):
        results = {}
        for b in self.benchmarks:
            results[b.name] = b.auths()

        return results

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
    parser.add_argument('dest', metavar='OUTPUT', nargs='?', default=DEST, type=str,
                        help=f'destination directory for results (default: {DEST})')
    parser.add_argument('-b', action='store_true', help='gather instrumentation statistics')
    parser.add_argument('-r', action='store_true', help='retain raw measurement data')
    parser.add_argument('-v', action='store_true', help='display commands when executing')
    args = parser.parse_args()

    verbose = args.v

    # Compile benchmarking utility module
    sp.check_call(['make', '-C', '.'])
    import bench

    # Recompile plugin if needed
    sp.check_call(['make', '-C', PLUGIN_DIR])

    # Make sure output directory exists
    os.makedirs(args.dest, exist_ok=True)

    suite = Suite(args.suite[0])

    # Do we gather build statistics this run?
    if args.b:
        inst = suite.build(PAC_ARGS_INST, True)
        auths = suite.auths()

        dump_inst(os.path.join(args.dest, 'build.csv'), inst, auths)
        exit(0)

    # Pure run without PAC
    suite.build(PAC_ARGS_BASE)
    data = suite.meas()

    dump_stat(os.path.join(args.dest, 'nopac.csv'), data)
    if args.r:
        dump_raw(os.path.join(args.dest, 'nopac_raw.csv'), data)

    # PAC-instrumented run
    suite.build(PAC_ARGS_INST)
    data = suite.meas()

    dump_stat(os.path.join(args.dest, 'pac.csv'), data)
    if args.r:
        dump_raw(os.path.join(args.dest, 'pac_raw.csv'), data)

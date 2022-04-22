#!/usr/bin/env python3

import subprocess as sp
import numpy as np
import argparse, os, sys
import csv, json
import bench

DEST = '.'

PLUGIN_PATH = os.path.abspath('../gcc/pac_sw_plugin.so')
PAC_MAPPER = os.path.abspath('../gcc/map_device.o')
PROLOGUE = os.path.abspath('../gcc/prologue.s')
EPILOGUE = os.path.abspath('../gcc/epilogue.s')

verbose = False

class Benchmark:
    def __init__(self, j, b):
        self.path = b['path']
        self.name = b.get('name', os.path.basename(self.path))
        self.warmup = b.get('warmup', j.get('warmup', 0))
        self.ntimes = b.get('ntimes', j['ntimes'])
        self.do_run = b.get('do_run', j['do_run'])
        self.do_build = b.get('do_build', j['do_build'])

    def do(self, cmd):
        if not verbose:
            cmd += ' > /dev/null'
        argv = ['sh', '-xc' if verbose else '-c', cmd]
        sp.check_call(argv, cwd=self.path)

    def build(self):
        self.do(self.do_build)

    def meas(self):
        diff = np.zeros(self.ntimes)

        cmd = self.do_run
        if not verbose:
            cmd += ' > /dev/null'

        cwd = os.getcwd()
        os.chdir(self.path)

        for i in range(-self.warmup, self.ntimes):
            sys.stdout.write(f'\r{self.name}: {i+1}/{self.ntimes:<12d}\r')
            sys.stdout.flush()
            res = bench.run(['/bin/sh', '-c', cmd])
            if i >= 0:
                diff[i] = res

        os.chdir(cwd)

        mean = diff.mean()
        std = diff.std()/mean*100
        masked = np.ma.masked_outside(diff, mean-2*std, mean+2*std)
        masked_count = np.ma.count_masked(masked)
        sys.stdout.write(f'{self.name}: {mean:#.9f} ({std:#.3f} %); {masked_count} outliers rejected\n')

        return masked

class Suite:
    def __init__(self, path):
        with open(path) as f:
            j = json.load(f)

        self.benchmarks = []
        for b in j['benchmarks']:
            self.benchmarks.append(Benchmark(j, b))

    def build(self):
        for b in self.benchmarks:
            b.build()
        # sp.check_call(["sudo", "sh", "-c", "sync; echo 3 > /proc/sys/vm/drop_caches"])

    def meas(self):
        results = {}
        for b in self.benchmarks:
            results[b.name] = b.meas()

        return results

def plugin_flags(plugin, args):
    name = ''.join(os.path.basename(plugin).split('.')[:-1])

    s = '-fplugin=' + plugin
    for k, v in args.items():
        s += ' -fplugin-arg-' + name + '-' + k + '=' + v

    return s

def dump(f, data):
    FIELDS = ['name', 'mean', 'std', 'min', 'max']
    with open(f, 'w') as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        for k, v in data.items():
            w.writerow([k, v.mean(), v.std(), v.min(), v.max()])

def dump_raw(f, data):
    with open(f, 'w') as f:
        w = csv.writer(f)
        v = data.values()
        maxlen = max([len(i) for i in v])
        w.writerow(data.keys())
        for i in range(maxlen):
            w.writerow([j[i] if i < len(j) else None for j in v])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compiler and run PAC-SW benchmarks.')
    parser.add_argument('suite', metavar='SUITE', nargs=1, type=str,
                        help='input file with benchmark suite description')
    parser.add_argument('dest', metavar='OUTPUT', nargs='?', default=DEST, type=str,
                        help=f'destination directory for results (default: {DEST})')
    parser.add_argument('-v', action='store_true', help='display commands when executing')
    parser.add_argument('-r', action='store_true', help='retain raw measurement data')

    args = parser.parse_args()
    verbose = args.v

    suite = Suite(args.suite[0])

    os.environ['PAC_MAPPER'] = PAC_MAPPER
    os.environ['PAC_FLAGS'] = plugin_flags(PLUGIN_PATH, { 'init-function': 'map_device' })

    print('Building without PA:')
    suite.build()
    data = suite.meas()

    # Write the results
    os.makedirs(args.dest, exist_ok=True)

    dump(os.path.join(args.dest, 'nopac.csv'), data)
    if args.r:
        dump_raw(os.path.join(args.dest, 'nopac_raw.csv'), data)

    os.environ['PAC_FLAGS'] = plugin_flags(PLUGIN_PATH, { 'init-function': 'map_device',
                                                          'prologue': PROLOGUE,
                                                          'epilogue': EPILOGUE })

    print('Building with PA:')
    suite.build()
    data = suite.meas()

    dump(os.path.join(args.dest, 'pac.csv'), data)
    if args.r:
        dump_raw(os.path.join(args.dest, 'pac_raw.csv'), data)

#!/usr/bin/python

import numpy as np
import subprocess as sp

import os, sys
import tempfile
import contextlib

import csv
import toml

from multiprocessing import cpu_count
from platform import uname

from versuchung.experiment import Experiment
from versuchung.types import String
from versuchung.files import File, CSV_File

import timing # timing.so

CUR_CPUFREQ  = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"
KPACD_DIR    = "/sys/kernel/debug/kpacd"
KPACD_NR_PAC = os.path.join(KPACD_DIR, "nr_pac")
KPACD_NR_AUT = os.path.join(KPACD_DIR, "nr_aut")

PLUGIN_DIR = os.path.join(sys.path[0], "../gcc")
PLUGIN_DLL = os.path.join(PLUGIN_DIR, "pac_sw_plugin.so")

@contextlib.contextmanager
def working_directory(path):
    """Changes working directory and returns to previous on exit."""
    prev_cwd = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(prev_cwd)

# Construct gcc flags for plugin and its arguments
def plugin_args(plugin, args):
    name = "".join(os.path.basename(plugin).split(".")[:-1])

    s = "-fplugin=" + plugin
    for k, v in args.items():
        s += " -fplugin-arg-" + name + "-" + k + "=" + v

    return s

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
        bench = j["benchmarks"][name]
        self.path = os.path.join(sys.path[0], bench["path"])
        self.warmup = bench.get("warmup", j["warmup"])
        self.samples = bench.get("samples", j["samples"])
        self.run_cmd = bench.get("run_cmd", j["run_cmd"])
        self.build_cmd = bench.get("build_cmd", j["build_cmd"])

    def build(self):
        sp.check_call(["/bin/sh", "-xc", self.build_cmd], cwd=self.path)

    # Measure average run duration of the benchmark and amount of
    # authentications
    def meas(self):
        nr_pac_0 = get_attr(KPACD_NR_PAC)
        nr_aut_0 = get_attr(KPACD_NR_AUT)
        durs = np.zeros(self.samples)

        with working_directory(self.path):
            for i in range(-self.warmup, self.samples):
                sys.stdout.write(f"\r{self.name}: {i+1}/{self.samples:<12d}\r")
                sys.stdout.flush()
                dur = timing.run(["/bin/sh", "-c", self.run_cmd])
                if i >= 0:
                    durs[i] = dur

        mean = durs.mean()
        rstd = durs.std()/mean*100
        sys.stdout.write(f"{self.name}: {mean:#.9f} ({rstd:#.3f} %)\n")

        nr_pac = get_attr(KPACD_NR_PAC) - nr_pac_0
        nr_aut = get_attr(KPACD_NR_AUT) - nr_aut_0
        assert nr_pac == nr_aut

        return durs, nr_pac//(self.warmup+self.samples)

class Suite:
    def __init__(self, path):
        with open(path) as f:
            j = toml.load(f)

        self.benchmarks = \
            [Benchmark(bench, j) for bench in j["benchmarks"].keys()]

    def build_nopac(self, cflags):
        os.environ["CFLAGS"] = cflags
        for b in self.benchmarks:
            b.build()

    # Build the benchmarks with specified plugin arguments, compile
    # instrumentation statistics
    def build_pac(self, cflags, args):
        stat = {}

        tmp = tempfile.NamedTemporaryFile(mode="r+")
        args["dump"] = tmp.name

        cflags += " " + plugin_args(PLUGIN_DLL, args)
        os.environ["CFLAGS"] = cflags

        for b in self.benchmarks:
            stat[b.name] = [0, 0]
            tmp.truncate(0)

            b.build()

            # Compile stats of this benchmark
            tmp.seek(0)
            for row in csv.reader(tmp):
                stat[b.name][0] += int(row[1])
                stat[b.name][1] += int(row[2])

        tmp.close()
        return stat

    def meas(self):
        results = {}
        auths = {}
        for b in self.benchmarks:
            results[b.name], auths[b.name] = b.meas()

        return results, auths

class Bench(Experiment):
    def get_arch(self):
        return String(uname().machine)

    def get_cpumasks(self):
        cpumasks = ""
        for i in range(cpu_count()):
            with open(os.path.join(KPACD_DIR, str(i), "cpumask")) as f:
                mask = f.read().strip()
                if not mask:
                    continue
                cpumasks += f"{i}={mask} "

        return String(cpumasks.strip())

    def get_backend(self):
        with open(os.path.join(KPACD_DIR, "backend")) as f:
            return String(f.read().strip())

    inputs = {
        "suite":	String("tacle-bench"),
        "cflags":	String("-O0"),
        "scope":	String("std"),
        "arch":		get_arch,
        "cpumasks":	get_cpumasks,
        "backend":	get_backend,
    }

    outputs = {
        "scaling_cur_freq":	File("scaling_cur_freq"),
        "build": 		CSV_File("build.csv"), # Build facts
    }

    def run(self):
        with open(CUR_CPUFREQ) as f:
            self.o.scaling_cur_freq.value = f.read()

        suite = Suite(os.path.join(sys.path[0], self.i.suite.value) + ".toml")

        # Baseline run without PAC
        suite.build_nopac(self.i.cflags.value)
        durs, _ = suite.meas()
        np.savez_compressed("nopac.npz", **durs)

        # PAC-protected run
        args = {}
        if self.i.scope.value:
            args["scope"] = self.i.scope.value

        inst = suite.build_pac(self.i.cflags.value, args)
        durs, auths = suite.meas()
        np.savez_compressed("pac.npz", **durs)

        self.o.build.append(["name", "inst", "total", "auths"])
        for k in inst.keys():
            self.o.build.append([k, inst[k][0], inst[k][1], auths[k]])

if __name__ == "__main__":
    import sys
    experiment = Bench()

    dirname = experiment(sys.argv + ["-s"])
    print(dirname)

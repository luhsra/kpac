#!/usr/bin/env python3

import numpy as np
import subprocess as sp

import os, sys
import tempfile
import contextlib
import re

import time

from pprint import pprint
from multiprocessing import cpu_count
from platform import uname

from versuchung.experiment import Experiment
from versuchung.types import String, Integer, Bool
from versuchung.files import File, CSV_File, Directory

from versuchung.execute import shell, shell_failok

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

def get_attr(attr):
    val = 0
    with open(attr) as f:
        val = int(f.read())
    return val

# Construct gcc flags for plugin and its arguments
def plugin_args(plugin, args):
    name = "".join(os.path.basename(plugin).split(".")[:-1])

    s = "-fplugin=" + plugin
    for k, v in args.items():
        s += " -fplugin-arg-" + name + "-" + k + "=" + v

    return s

class Memtier(Experiment):
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

    def get_env(var):
        return lambda self: String(os.environ.get(var, ""))

    inputs = {
        "scope":   String("nil"),
        "variant": String("syscall"),

        "cpumasks": get_cpumasks,
        "backend":  get_backend,

        "arch":   lambda self: String(uname().machine),
        "host":   lambda self: String(uname().node),
        "kernel": lambda self: String(" ".join([
            uname().system, uname().release, uname().version
        ])),

        "memcached":   Directory("../memcached"),
        "memtier":     Directory("../memtier_benchmark"),

        "memtier_args": String("--test-time=100"),

        "save_samples": Bool(False),

        "server_threads": String("32"),
        "client_threads": String("32"),
        "server_cpus": String("0-31"),
        "client_cpus": String("32-63"),

        "cflags":      get_env("CFLAGS"),
        "ld_preload":  get_env("LD_PRELOAD"),
        "libkpac_mode": get_env("LIBKPAC_MODE"),
    }

    outputs = {
        "scaling_cur_freq": File("scaling_cur_freq"),
        "log":              Directory("log"),
        "HDR":              File("HDR"),
        "json":             File("output.json"),
        "latency":          File("latency.npz"),
        "output":           File("output"),
        "nr_pac":           File("nr_pac"),
        "nr_aut":           File("nr_aut"),
        "libkpac":          File("libkpac.csv"),
        "size":             File("size"),
        "build":            CSV_File("build.csv"), # Build facts
    }

    def run(self):
        pprint(self.i)

        try:
            with open(CUR_CPUFREQ) as f:
                self.o.scaling_cur_freq.value = f.read()
        except EnvironmentError:
            print("Cannot detect CPU frequency")
            self.o.scaling_cur_freq.value = 0

        shell.track(self.o.log.path)

        pid = os.getpid()

        nr_pac_0 = get_attr(KPACD_NR_PAC)
        nr_aut_0 = get_attr(KPACD_NR_AUT)

        with working_directory(self.i.memcached.path):
            args = {}
            args["asm"] = os.path.join(PLUGIN_DIR, "asm", self.i.variant.value, self.i.arch.value)
            args["dump"] = self.o.build.path
            if self.i.scope.value:
                args["scope"] = self.i.scope.value

            plugin_flags = ""
            if self.i.scope.value != 'nil':
                plugin_flags = " " + plugin_args(PLUGIN_DLL, args)

            shell("git clean -ffxd")
            shell("./autogen.sh")
            shell("./configure \"CFLAGS=" + self.i.cflags.value + plugin_flags + "\"")
            shell("make -j$(nproc)")

            with open(self.o.size.path, "w") as f:
                sp.check_call(["size", "./memcached"], stdout=f)

            shell("taskset -cp " + self.i.server_cpus.value + " " + str(pid))

            os.environ['LIBKPAC_STAT'] = self.o.libkpac.path

            logpath = os.path.join(self.o.log.path, "memcached.log");
            with open(logpath, 'w') as logfile:
                memcached = sp.Popen(["./memcached", "-p", "22122", "-t", self.i.server_threads.value,
                                      "-o", "maxconns_fast=0", "-b", "32768", "-c", "32768"
                                      ],
                                     stdout=logfile, stderr=logfile)

            del os.environ['LIBKPAC_STAT']

        time.sleep(5)

        with working_directory(self.i.memtier.path):
            shell("taskset -cp " + self.i.client_cpus.value + " " + str(pid))

            get_samples = tempfile.NamedTemporaryFile(mode="r+")
            sp.check_call("./memtier_benchmark -p 22122 -t " + self.i.client_threads.value +
                          " -P memcache_text --print-percentiles 50,99,99.5,99.9" +
                          " " + self.i.memtier_args.value +
                          " --json-out-file=\"" + self.o.json.path + "\"" +
                          " --hdr-file-prefix=\"" + self.o.HDR.path + "\"" +
                          " -o \"" + self.o.output.path + "\"" +
                          (" --get-samples=\"" + get_samples.name + "\"" if self.i.save_samples.value else ""),
                          shell=True)

            if self.i.save_samples.value:
                a = np.genfromtxt(get_samples.name)
                np.savez_compressed(self.o.latency.path, a)

        memcached.terminate()

        time.sleep(3)

        self.o.nr_pac.value = str(get_attr(KPACD_NR_PAC) - nr_pac_0)
        self.o.nr_aut.value = str(get_attr(KPACD_NR_AUT) - nr_aut_0)


if __name__ == "__main__":
    experiment = Memtier()
    dirname = experiment(sys.argv)
    print(dirname)

#!/usr/bin/env python3

import numpy as np
import subprocess as sp

import os, sys
import tempfile
import contextlib
import re

import shutil
import time

from pprint import pprint
from multiprocessing import cpu_count
from platform import uname

from versuchung.experiment import Experiment
from versuchung.types import String, Integer, Bool
from versuchung.files import File, CSV_File, Directory

from versuchung.execute import shell, shell_failok

KPACD_DIR    = "/sys/kernel/debug/kpacd"

@contextlib.contextmanager
def working_directory(path):
    """Changes working directory and returns to previous on exit."""
    prev_cwd = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(prev_cwd)

class Latency(Experiment):
    def get_backend(self):
        with open(os.path.join(KPACD_DIR, "backend")) as f:
            return String(f.read().strip())

    def get_env(var):
        return lambda self: String(os.environ.get(var, ""))

    inputs = {
        "variant": String("syscall"),
        "backend": get_backend,
        "runs": Integer(32000000),
        "csv": Bool(False),

        "arch":   lambda self: String(uname().machine),
        "host":   lambda self: String(uname().node),
        "kernel": lambda self: String(" ".join([
            uname().system, uname().release, uname().version
        ])),
    }

    outputs = {
        "samples": File("samples.npz"),
        "samples_csv": File("samples.csv"),
    }

    def run(self):
        pprint(self.i)

        flag = {
            'pac-pl': '-l',
            'kpacd': '-d',
            'syscall': '-s'
        }

        with working_directory(sys.path[0]):
            samples = tempfile.NamedTemporaryFile(mode="r+")
            cmd = f"./latency {flag[self.i.variant.value]} -n {self.i.runs.value} {samples.name}"
            print(cmd)
            sp.check_call(cmd, shell=True)

            if not self.i.csv.value:
                a = np.genfromtxt(samples.name)
                np.savez_compressed(self.o.samples.path, a)
            else:
                shutil.copyfile(samples.name, self.o.samples_csv.path)


if __name__ == "__main__":
    experiment = Latency()
    dirname = experiment(sys.argv)
    print(dirname)

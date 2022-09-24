# Software-Emulated Pointer Authentication

This repository contains the source code of the PAC-SW plugin for the bachelor's thesis *"[Software-Emulated Pointer Authentication for Control-Flow Integrity Protection](https://scm.sra.uni-hannover.de/theses/2022/BA_ill.ostapyshyn)."*

## Structure

* `bench/`: Evaluation kit based on synthetic benchmarks.

  The evaluation script uses Versuchung and executes benchmark recipes described in `.toml` format.
  The Python module `timing.so` measures fork-exec-wait cycle using the monotonic system clock and is built using the `Makefile`.
  Also provides compatibility patches for the Cortexsuite benchmark in `patches/cortexsuite`.
  The scripts in the `setup` directory facilitate noiseless measurements by disabling turbo boost and frequency scaling.

* `cve/`: Analysis of the past vulnerabilities in the GNU C library.

* `gcc/`: The PAC-SW plugin for GCC.

  Contains the source code of the plugin as well as the assembly snippets (`asm/`) for code instrumentation.
  Testing suite can be found in the `tests/` directory.

* `memtier/`: Evaluation kit based on `memcached` and `memtier_benchmark`.

  Contains energy and performance overhead evaluation scripts, data and the analysis notebooks.
  The patch in this directory enable `memtier_benchmark` to output raw latency samples into a file.

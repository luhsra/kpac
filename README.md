# Software-Emulated Pointer Authentication

This repository contains the source code of the userspace part of the KPAC mechanism described in [KPAC: Efficient Emulation of the ARM Pointer Authentication Instructions](https://sra.uni-hannover.de/Publications/2024/ostapyshyn_24_emsoft.pdf) paper presented at EMSOFT2024.
Please refer to https://github.com/luhsra/linux-kpac for the Linux kernel modifications.

## Structure

* `eval/`: Evaluation kit and the artifacts
* `gcc/`: The GCC plugin for static instrumentation
* `libkpac/`: Load-time patching library (inserted via `LD_PRELOAD`)
* `pac-pl/`: PAC-PL initialization library (inserted via `LD_PRELOAD`)

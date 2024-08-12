# Software-Emulated Pointer Authentication

This repository contains the source code of the userspace part of the KPAC mechanism.
Please refer to https://github.com/luhsra/linux-kpac for the Linux kernel modifications.

## Structure

* `eval/`: Evaluation kit and the artifacts
* `gcc/`: The GCC plugin for static instrumentation
* `libkpac/`: Load-time patching library (inserted via `LD_PRELOAD`)
* `pac-pl/`: PAC-PL initialization library (inserted via `LD_PRELOAD`)

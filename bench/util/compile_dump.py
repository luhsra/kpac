#!/usr/bin/env python3

import sys
import csv
import os


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Input file not specified.")
        exit(1)
    inp = sys.argv[1]

    dump = {}
    stat = {}

    with open(inp) as f:
        reader = csv.reader(f)
        for row in reader:
            if row[0] not in dump:
                dump[row[0]] = [int(x) for x in row[1:]]
            else:
                print("Duplicate entries! Check your files.")
                exit(1)

    for k, v in dump.items():
        name = k.split(os.sep)[-2]
        if name not in stat:
            stat[name] = v
        else:
            stat[name] = [sum(x) for x in zip(stat[name], v)]

    writer = csv.writer(sys.stdout)
    writer.writerow(['Benchmark', 'Instrumented', 'Total'])
    for k, v in stat.items():
        writer.writerow([k] + v)

#!/usr/bin/env python3
"""Keep the first queries in each density bin that eLFTJ finishes on 1 thread within 30 s.

Usage: python3 tools/screen_queries.py <thesis binary> <graph.txt> <count> <bin_dir>...

Runs with delivery on, in file order, and stops a bin once <count> queries pass.
Writes <bin_dir>/screened.txt: one "<query file> <seconds>" line per passing query.
"""

import os
import re
import subprocess
import sys
import tempfile

binary, graph, count, *bin_dirs = sys.argv[1:]
count = int(count)

for bin_dir in bin_dirs:
    names = os.listdir(bin_dir)
    queries = [f for f in names if f.endswith(".txt") and f != "screened.txt"]
    # Sort by query number, so Q10-10.txt comes after Q10-9.txt.
    queries.sort(key=lambda f: int(re.search(r"-(\d+)\.txt$", f)[1]))

    passed = []
    tried = 0
    for name in queries:
        tried += 1
        with tempfile.NamedTemporaryFile(suffix=".log") as log:
            command = [binary, "-d", graph, "-q", os.path.join(bin_dir, name)]
            command += ["-QorCandi", "C", "-split", "linear"]
            command += ["-schedule", "busy2idlenostop"]
            command += ["-BackMethods", "DPiso_DPiso_LFTJ", "-mode", "match"]
            command += ["-sink", "threadlocal", "-threadnums", "1", "-num", "MAX"]
            command += ["-time_limit", "30", "-OutputFile", log.name]
            subprocess.run(command, stdout=subprocess.DEVNULL, check=False)
            with open(log.name) as f:
                text = f.read()

        overtime = re.findall(r"Overtime: (\d+)", text)
        if overtime and overtime[-1] == "0":
            seconds = 0
            for timer in ["Enumerate time", "Consolidation time"]:
                seconds += float(re.findall(timer + r" \(seconds\): (\S+)", text)[-1])
            passed.append(f"{name} {seconds:.3f}")
            if len(passed) == count:
                break

    with open(os.path.join(bin_dir, "screened.txt"), "w") as out:
        out.write("\n".join(passed) + "\n")
    print(f"{bin_dir}: {len(passed)} of {tried} tried passed")

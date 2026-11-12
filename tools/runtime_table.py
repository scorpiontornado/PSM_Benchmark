#!/usr/bin/env python3
"""Print each schedule's speed against its baseline from a runtime comparison's results.

Usage: python3 tools/runtime_table.py <results.csv>

The baseline is pEC-cBNS with the same kernel and thread count. For the 1-thread runs
with -split1thread it is the 1-thread run without splitting (NOSPLIT), so their ratio
is the cost of the runtime's task creation alone.
A ratio above 1 means the schedule delivers more embeddings per second.
"""

import csv
import statistics
import sys

# (graph, combination, threads, query) -> each run's EPS, task count, split share
# and time per task.
cells = {}
with open(sys.argv[1]) as f:
    for r in csv.DictReader(f):
        if r["delivery"] != "delivered" or r["overtime"] != "0" or not r["eps"]:
            continue

        threads = r["threads"]
        if "-split1thread" in r["flags"]:
            threads = "1 split"

        split_share = 0
        if float(r["thread_exec_s"] or 0) > 0:
            split_share = float(r["thread_split_s"] or 0) / float(r["thread_exec_s"])

        # Time in the split functions per task created, in microseconds.
        task_us = 0
        if int(r["task_num"] or 0) > 0:
            task_us = float(r["thread_split_s"] or 0) / int(r["task_num"]) * 1e6

        graph = r["data"].split("/")[-1].removesuffix(".txt")
        key = (graph, r["combination"], threads, r["query"])
        run = (float(r["eps"]), int(r["task_num"] or 0), split_share, task_us)
        cells.setdefault(key, []).append(run)

# (graph, combination, threads) -> one (ratio, tasks, split share, time per task)
# per query.
table = {}
for (graph, combination, threads, query), runs in cells.items():
    kernel = combination.split("-")[-1]
    baseline = (graph, f"pEC-cBNS-{kernel}", threads, query)
    if threads == "1 split":
        baseline = (graph, f"NOSPLIT-{kernel}", "1", query)
    if baseline not in cells or baseline[1] == combination:
        continue

    eps = statistics.median(run[0] for run in runs)
    baseline_eps = statistics.median(run[0] for run in cells[baseline])
    tasks = statistics.median(run[1] for run in runs)
    split_share = statistics.median(run[2] for run in runs)
    task_us = statistics.median(run[3] for run in runs)
    row = (eps / baseline_eps, tasks, split_share, task_us)
    table.setdefault((graph, combination, threads), []).append(row)

print("median over queries (min-max for the EPS ratio)")
header = f"{'graph':12} {'combination':20} {'threads':8} {'EPS ratio':22}"
print(header, "tasks    split/exec  us/task")
for (graph, combination, threads), per_query in sorted(table.items()):
    ratios = [row[0] for row in per_query]
    ratio = f"{statistics.median(ratios):.2f} ({min(ratios):.2f}-{max(ratios):.2f})"
    tasks = statistics.median(row[1] for row in per_query)
    split_share = statistics.median(row[2] for row in per_query)
    name = f"{graph:12} {combination:20} {threads:8} {ratio:22}"
    task_us = statistics.median(row[3] for row in per_query)
    print(name, f"{tasks:<8.0f} {split_share:<11.2f} {task_us:.1f}")

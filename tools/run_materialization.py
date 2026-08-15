#!/usr/bin/env python3
"""Test the materialization modes (none, globallock, threadlocal) and write a CSV.

    python3 tools/run_materialization.py <graph.txt> <query.txt> -o results.csv

Runs every mode at every thread count and records throughput, memory and the
sink's own counters, one row per run. Each run is a separate process so peak
RSS is per-run rather than a running maximum.
"""

import argparse
import csv
import re
import subprocess
import sys

MODES = ["none", "globallock", "threadlocal"]

# Time limit per thread count. Shorter at high thread counts so a run does not
# store more than ~40 GB. All modes share a window so the warm-up at the start
# of a run is the same fraction of each.
WINDOWS = {1: 30, 2: 27, 4: 13, 8: 6}

PATTERNS = {
    "embeddings": r"#Embeddings: (\d+)",
    "enumerate_s": r"Enumerate time \(seconds\): ([\d.eE+-]+)",
    "call_count": r"Call Count: (\d+)",
    "per_call_ns": r"Per Call Time \(nanoseconds\): (\d+)",
    "overtime": r"Overtime: (\d+)",
    "stored_embeddings": r"stored embeddings: (\d+)",
    "stored_bytes": r"stored bytes: (\d+)",
    "worker_sinks": r"worker sinks: (\d+)",
    "peak_rss_kb": r"Maximum resident set size \(kbytes\): (\d+)",
    "sys_s": r"System time \(seconds\): ([\d.]+)",
    "minor_faults": r"Minor \(reclaiming a frame\) page faults: (\d+)",
}

COLUMNS = ["mode", "threads", "window_s", "embeddings", "enumerate_s", "eps",
           "call_count", "per_call_ns", "overtime", "stored_embeddings",
           "stored_bytes", "worker_sinks", "peak_rss_kb", "sys_s", "minor_faults"]


def run_one(args, mode, threads, log_path):
    seconds = WINDOWS[threads]
    proc = subprocess.run([
        "/usr/bin/time", "-v", args.binary,
        "-d", args.data, "-q", args.query,
        "-QorCandi", "C", "-split", "linear", "-schedule", "busy2idlenostop",
        "-BackMethods", "DPiso_DPiso_LFTJ",
        "-threadnums", str(threads), "-num", "MAX", "-time_limit", str(seconds),
        "-materialize", mode, "-OutputFile", log_path,
    ], capture_output=True, text=True, check=False)

    with open(log_path) as handle:
        text = handle.read() + proc.stderr
    row = {"mode": mode, "threads": threads, "window_s": seconds}
    for name, pattern in PATTERNS.items():
        found = re.findall(pattern, text)
        row[name] = found[-1] if found else ""

    try:
        row["eps"] = round(int(row["embeddings"]) / float(row["enumerate_s"]))
    except (ValueError, ZeroDivisionError):
        row["eps"] = ""
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("data")
    parser.add_argument("query")
    parser.add_argument("-o", "--out", default="materialization.csv")
    parser.add_argument("--binary", default="./build/matching/SubgraphMatching.out")
    args = parser.parse_args()

    rows = []
    for threads in sorted(WINDOWS):
        for mode in MODES:
            log_path = f"{args.out}.{mode}-t{threads}.log"
            print(f"run: {mode:11s} threads={threads} window={WINDOWS[threads]}s",
                  flush=True)
            row = run_one(args, mode, threads, log_path)
            rows.append(row)

            # A binary built before the probe accepts -materialize and ignores it.
            if mode != "none" and not row["stored_embeddings"]:
                sys.exit(f"{mode} stored nothing -- rebuild {args.binary}")
            rss_gb = int(row["peak_rss_kb"] or 0) / 1e6
            print(f"     eps={row['eps']} peak_rss={rss_gb:.1f} GB", flush=True)

    with open(args.out, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {len(rows)} rows to {args.out}")


if __name__ == "__main__":
    main()

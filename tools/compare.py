#!/usr/bin/env python3
"""Run one binary over the split-mode x delivery matrix and write a CSV.

    python3 tools/compare.py <binary> <label> <graph.txt> <query.txt> -o <outdir>

Everything for one run lands in <outdir>/<label>/: one CSV and one driver log
per configuration. The label keeps a control run and a thesis run apart, and
re-running the same label refuses to start rather than overwriting. Pass
--baseline for a binary built from the baseline-upstream tag: it predates -mode
and -sink, so it only gets the two split modes and no delivery flags.

Runs are time-bounded, never -num bounded: -num is a per-task limit, so above
one thread the output overshoots by up to the thread count. All configurations
at a given thread count share one window, so the fixed warm-up is the same
fraction of each. Windows shrink as threads rise to keep stored output inside
the memory budget; throughput is a rate, so that costs sample size, not
comparability.

Each run is a separate process, so peak RSS is per-run rather than a running
maximum.
"""

import argparse
import csv
import os
import re
import subprocess
import sys

# (config name, -QorCandi, -mode, -sink)
CONFIGS = [
    ("q_globallock", "Q", "match", "globallock"),
    ("q_threadlocal", "Q", "match", "threadlocal"),
    ("c_count", "C", "count", "globallock"),
    ("c_globallock", "C", "match", "globallock"),
    ("c_threadlocal", "C", "match", "threadlocal"),
]

BASELINE_CONFIGS = [
    ("q_baseline", "Q", "", ""),
    ("c_baseline", "C", "", ""),
]

# Time limit per thread count, shared by every configuration at that count.
WINDOWS = {1: 30, 2: 27, 4: 13, 8: 6}

PATTERNS = {
    "embeddings": r"#Embeddings: (\d+)",
    "enumerate_s": r"Enumerate time \(seconds\): ([\d.eE+-]+)",
    "consolidation_s": r"Consolidation time \(seconds\): ([\d.eE+-]+)",
    "join_s": r"Join time \(seconds\): ([\d.eE+-]+)",
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

COLUMNS = [
    "label",
    "source_version",
    "data",
    "query",
    "config",
    "split",
    "mode",
    "sink",
    "threads",
    "window_s",
    "embeddings",
    "enumerate_s",
    "consolidation_s",
    "join_s",
    "eps",
    "call_count",
    "per_call_ns",
    "overtime",
    "stored_embeddings",
    "stored_bytes",
    "worker_sinks",
    "peak_rss_kb",
    "sys_s",
    "minor_faults",
]


def source_version(binary):
    """git describe of the tree the binary was built from, so a row is traceable."""
    proc = subprocess.run(
        [
            "git",
            "-C",
            os.path.dirname(os.path.abspath(binary)),
            "describe",
            "--tags",
            "--always",
            "--dirty",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.stdout.strip() or "unknown"


def run_one(args, config, threads, log_path):
    name, split, mode, sink = config
    seconds = WINDOWS[threads]
    command = [
        "/usr/bin/time",
        "-v",
        args.binary,
        "-d",
        args.data,
        "-q",
        args.query,
        "-QorCandi",
        split,
        "-split",
        "linear",
        "-schedule",
        "busy2idlenostop",
        "-BackMethods",
        "DPiso_DPiso_LFTJ",
        "-threadnums",
        str(threads),
        "-num",
        "MAX",
        "-time_limit",
        str(seconds),
        "-OutputFile",
        log_path,
    ]
    if mode:
        command += ["-mode", mode, "-sink", sink]
    proc = subprocess.run(command, capture_output=True, text=True, check=False)

    with open(log_path) as handle:
        text = handle.read() + proc.stderr
    row = {
        "label": args.label,
        "data": args.data,
        "query": args.query,
        "config": name,
        "split": split,
        "mode": mode,
        "sink": sink,
        "threads": threads,
        "window_s": seconds,
    }
    for field, pattern in PATTERNS.items():
        found = re.findall(pattern, text)
        row[field] = found[-1] if found else ""

    # QSplit's embeddings do not exist until the join finishes, and a
    # thread-local sink defers delivery to consolidation, so both count.
    delivery_s = sum(
        float(row[f] or 0) for f in ("enumerate_s", "consolidation_s", "join_s")
    )
    try:
        row["eps"] = round(int(row["embeddings"]) / delivery_s)
    except (ValueError, ZeroDivisionError):
        row["eps"] = ""
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("label")
    parser.add_argument("data")
    parser.add_argument("query")
    parser.add_argument("-o", "--out", default="results")
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()

    run_dir = os.path.join(args.out, args.label)
    if os.path.exists(run_dir):
        sys.exit(f"{run_dir} already exists -- pick another label")
    os.makedirs(run_dir)
    csv_path = os.path.join(run_dir, "compare.csv")

    version = source_version(args.binary)
    configs = BASELINE_CONFIGS if args.baseline else CONFIGS

    # Written as the sweep goes, so a run that dies partway keeps what it has.
    handle = open(csv_path, "w", newline="")
    writer = csv.DictWriter(handle, fieldnames=COLUMNS)
    writer.writeheader()

    count = 0
    for threads in sorted(WINDOWS):
        for config in configs:
            name = config[0]
            log_path = os.path.join(run_dir, f"{name}-t{threads}.log")
            print(
                f"run: {name:14s} threads={threads} window={WINDOWS[threads]}s",
                flush=True,
            )
            row = run_one(args, config, threads, log_path)
            row["source_version"] = version
            writer.writerow(row)
            handle.flush()
            count += 1

            if not row["embeddings"]:
                # The run died before logging its statistics; the log file says why.
                print(f"     FAILED -- see {log_path}", flush=True)
                continue
            # A binary built before the rework accepts -mode and ignores it.
            if row["mode"] == "match" and not row["stored_embeddings"]:
                sys.exit(f"{name} stored nothing -- rebuild {args.binary}")
            rss_gb = int(row["peak_rss_kb"] or 0) / 1e6
            print(
                f"     eps={row['eps']} peak_rss={rss_gb:.1f} GB "
                f"overtime={row['overtime']}",
                flush=True,
            )

    handle.close()
    print(f"wrote {count} rows to {csv_path}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Write the plan CSV for one Thesis C experiment, for run_plan.py.

Usage: python3 tools/make_plan.py <experiment> <datasets_dir> <plan.csv> [max_threads]

Run from the repo root. Experiments on the main graphs read the screened.txt files
written by screen_queries.py. max_threads (default 64) drops the processes above it,
for machines where 64 threads aren't allowed.
"""

import csv
import os
import sys

THESIS = "build/matching/SubgraphMatching.out"
REPAIRED = "../PSM_baseline_repaired/build/matching/SubgraphMatching.out"
UPSTREAM = "../PSM_baseline/build/matching/SubgraphMatching.out"

# Enumeration kernels: sequential backtracking methods given as filter_order_enumerate.
KERNELS = {
    "eLFTJ": "DPiso_DPiso_LFTJ",
    "eGQL": "GQL_GQL_GQL",
    "eCFL": "CFL_CFL_EXPLORE",
    "eCECI": "CECI_CECI_CECI",
    "eDPiso": "DPiso_DPiso_DPiso",
    "eCIR": "DPiso_CIRCINUS_CIRCINUS",
}

# The kernels Yu et al. run under QSplit (their Figure 2).
QSPLIT_KERNELS = ["eLFTJ", "eGQL", "eCFL", "eDPiso"]

GSPLIT_SPLITS = {
    "pUO": "upperone",
    "pEC": "linear",
    "pFL": "layer",
    "pWA": "workload",
}

GSPLIT_SCHEDULES = {
    "cBNS": "busy2idlenostop",
    "cBDS": "busy2idledepthstop",
    "cTO": "timeout",
}

# The artifact runs the static schedules only with -split layer.
GSPLIT_STATIC = {
    "pFL-cST": "-split layer -schedule static",
    "pWA-cST": "-split layer -schedule staticworkload",
}

QSPLIT_PATTERNS = {
    "pSW": "STAR",
    "pTW": "TWINTWIG",
    "pCJ": "SEED",
    "pST": "SKETCHTREE",
}

# The oneTBB schedules, each mapped to its splits and the flags that replace -schedule.
# TODO: add the oneTBB schedules as they're implemented.
# E.g. `"cOBNS": (["pEC", "pFL"], "-schedule ...")`
ONETBB = {}

# The oneTBB ablations, each mapped to the flags that replace -schedule.
# They run only under pEC with eLFTJ and eCFL.
# TODO: add cBNS-cq and cOBNS-lazy as they're implemented.
# E.g. `"cBNS-cq": "-schedule ..."`
ONETBB_ABLATIONS = {}

# The schedules in the runtime comparison, besides the ablations.
# TODO: add the 1-thread oneTBB runs with splitting on, once the driver has a flag for them.
RUNTIME_SCHEDULES = ["cBNS", "cBDS", "cOBNS", "cOBDS"]

# The graph comparison: ranks 1, 11, ..., 51 of HPRD's delivered sweep at 16 threads.
# TODO: fill in from the HPRD sweep; these two are placeholders.
GRAPH_COMBOS = ["pEC-cBNS-eCFL", "pEC-cBNS-eCIR"]

# HPRD plus the two main graphs that span the widest range of output rates.
# TODO: pick the two graphs after the noise, pairing, load, inprocess and smoke runs.
# dblp and youtube are placeholders.
DELIVERY_GRAPHS = [("HPRD", "15"), ("dblp", "15"), ("youtube", "15")]

MAIN = [
    ("HPRD", "15"),
    ("human", "15"),
    ("dblp", "15"),
    ("youtube", "15"),
    ("HPRD", "native"),
    ("human", "native"),
]
LARGE = [("orkut", "15"), ("twitch", "15"), ("ldbc-sf10", "native")]
THREADS = [2, 4, 8, 16, 32, 64]

# Query density bins. Experiments that need a single query take it from bin 2.
BINS = ["1", "2", "3"]

# The extra flags for each delivery column.
# Yu et al.'s setup is what they measured: GSplit counts without storing.
# The two controls don't take delivery flags.
DELIVERY_FLAGS = {
    "delivered": "-mode match -sink threadlocal",
    "yu-setup": "-mode count -sink globallock",
    "count": "-mode count -sink globallock",
    "globallock": "-mode match -sink globallock",
    "threadlocal": "-mode match -sink threadlocal",
    "repaired": "",
    "upstream": "",
}
BOTH_COLUMNS = ["yu-setup", "delivered"]

# Yu et al.'s 100 combinations, named as in their paper:
# split-schedule-kernel under GSplit, pattern-kernel under QSplit.
YU_COMBOS = {}
for p, split in GSPLIT_SPLITS.items():
    for c, schedule in GSPLIT_SCHEDULES.items():
        for e, method in KERNELS.items():
            YU_COMBOS[f"{p}-{c}-{e}"] = (
                f"-QorCandi C -split {split} -schedule {schedule} -BackMethods {method}"
            )
for pc, flags in GSPLIT_STATIC.items():
    for e, method in KERNELS.items():
        YU_COMBOS[f"{pc}-{e}"] = f"-QorCandi C {flags} -BackMethods {method}"
for p, pattern in QSPLIT_PATTERNS.items():
    for e in QSPLIT_KERNELS:
        YU_COMBOS[f"{p}-{e}"] = (
            f"-QorCandi Q -Qpattern {pattern} -split linear -schedule busy2idlenostop "
            f"-BackMethods {KERNELS[e]}"
        )

ONETBB_COMBOS = {}
for c, (splits, flags) in ONETBB.items():
    for p in splits:
        for e, method in KERNELS.items():
            ONETBB_COMBOS[f"{p}-{c}-{e}"] = (
                f"-QorCandi C -split {GSPLIT_SPLITS[p]} {flags} -BackMethods {method}"
            )

ABLATION_COMBOS = {}
for c, flags in ONETBB_ABLATIONS.items():
    for e in ["eLFTJ", "eCFL"]:
        ABLATION_COMBOS[f"pEC-{c}-{e}"] = (
            f"-QorCandi C -split linear {flags} -BackMethods {KERNELS[e]}"
        )

ALL_COMBOS = YU_COMBOS | ONETBB_COMBOS | ABLATION_COMBOS

# pEC-cBNS with eLFTJ and eCFL, used by the noise, pairing and in-process experiments.
PILOT_COMBOS = {n: YU_COMBOS[n] for n in ["pEC-cBNS-eLFTJ", "pEC-cBNS-eCFL"]}

experiment, datasets, out_path = sys.argv[1:4]
max_threads = 64
if len(sys.argv) > 4:
    max_threads = int(sys.argv[4])
rows = []


def graph_path(name, labels):
    return f"{datasets}/{name}/L{labels}/{name}-{labels}.txt"


def screened_queries(name, labels, per_bin, bins=BINS):
    """The first `per_bin` queries that passed screening in each density bin."""
    queries = []
    for b in bins:
        pool = f"{datasets}/{name}/L{labels}/Q10/bin{b}"
        with open(f"{pool}/screened.txt") as f:
            passed = [line.split()[0] for line in f if line.strip()]
        queries += [f"{pool}/{query}" for query in passed[:per_bin]]
    return queries


def first_queries(name, labels, size=10, bins=BINS):
    """The first generated query in each density bin, unscreened.

    A bin can be empty on a dense graph, where random walks rarely give sparse queries.
    """
    paths = [f"{datasets}/{name}/L{labels}/Q{size}/bin{b}/Q{size}-1.txt" for b in bins]
    return [p for p in paths if os.path.exists(p)]


def pick(combos, names):
    """The named combinations, skipping any oneTBB schedule not implemented yet."""
    return {n: combos[n] for n in names if n in combos}


def cells(combos, threads):
    """Every combination at every thread count, as (name, flags, threads)."""
    return [(n, f, t) for n, f in combos.items() for t in threads]


def with_one_thread(combos, threads=THREADS):
    """Every combination at each thread count, plus the distinct 1-thread runs.

    At 1 thread the driver turns GSplit's splitting off, so the GSplit combinations
    become one run per kernel; QSplit stays the same.
    """
    one_thread = {}
    for name, flags in combos.items():
        if "-QorCandi Q" in flags:
            one_thread[name] = flags
        else:
            kernel = name.split("-")[-1]
            one_thread[f"NOSPLIT-{kernel}"] = (
                "-QorCandi C -split linear -schedule busy2idlenostop "
                f"-BackMethods {KERNELS[kernel]}"
            )
    return cells(combos, threads) + cells(one_thread, [1])


def add_processes(
    data,
    queries,
    runs,
    deliveries,
    copies,
    binary=THESIS,
    time_limit=30,
    cpus="",
    repeats=1,
):
    """Add one plan row per process to rows.

    Each query, run and delivery column gets `copies` identical processes,
    and each process repeats its run `repeats` times.
    """
    for query in queries:
        for name, flags, threads in runs:
            for delivery in deliveries:
                # TODO: deliver eCIR once the Circinus expansion is validated.
                if delivery == "delivered" and name.endswith("-eCIR"):
                    continue

                delivery_flags = DELIVERY_FLAGS[delivery]
                # QSplit cannot count, so Yu et al.'s setup stores it through one lock.
                if delivery == "yu-setup" and "-QorCandi Q" in flags:
                    delivery_flags = DELIVERY_FLAGS["globallock"]

                for _ in range(copies):
                    rows.append(
                        {
                            "experiment": experiment,
                            "combination": name,
                            "delivery": delivery,
                            "data": data,
                            "query": query,
                            "threads": threads,
                            "repeats": repeats,
                            "cpus": cpus,
                            "binary": binary,
                            "flags": f"{flags} {delivery_flags} -time_limit {time_limit}",
                        }
                    )


match experiment:
    # 10 separate processes per cell, so the noise includes variation between processes,
    # not just between runs in one process.
    case "noise":
        queries = screened_queries("HPRD", "15", 1, bins=["2"])
        add_processes(
            graph_path("HPRD", "15"),
            queries,
            cells(PILOT_COMBOS, [1, 16, 32]),
            BOTH_COLUMNS,
            copies=10,
        )

    # Alone on socket 0, alone on socket 1, then paired.
    # Run with --pair, which pairs the rows that have no CPU set.
    case "pairing":
        for name, labels in [("HPRD", "15"), ("dblp", "15"), ("youtube", "15")]:
            queries = screened_queries(name, labels, 1, bins=["2"])
            for threads, cpu_sets in [
                (4, ["0-3", "16-19", ""]),
                (16, ["0-15", "16-31", ""]),
            ]:
                for cpus in cpu_sets:
                    add_processes(
                        graph_path(name, labels),
                        queries,
                        cells(PILOT_COMBOS, [threads]),
                        BOTH_COLUMNS,
                        copies=5,
                        cpus=cpus,
                    )

    # One process per graph and column, to read the driver's graph load time.
    case "load":
        for name, labels in MAIN + LARGE:
            add_processes(
                graph_path(name, labels),
                first_queries(name, labels, bins=["2"]),
                cells(pick(YU_COMBOS, ["pEC-cBNS-eLFTJ"]), [16]),
                BOTH_COLUMNS,
                copies=1,
            )

    # 6 processes of 3 runs against 18 processes of 1, to measure what running the large
    # graphs' repeats inside one process costs.
    case "inprocess":
        for name, labels in [("HPRD", "15"), ("dblp", "15")]:
            queries = screened_queries(name, labels, 1, bins=["2"])
            data = graph_path(name, labels)
            add_processes(
                data,
                queries,
                cells(PILOT_COMBOS, [16]),
                ["delivered"],
                copies=6,
                repeats=3,
            )
            add_processes(
                data, queries, cells(PILOT_COMBOS, [16]), ["delivered"], copies=18
            )

    # Every combination once at 1 and 64 threads, to catch crashes before a long batch.
    case "smoke":
        add_processes(
            graph_path("HPRD", "15"),
            screened_queries("HPRD", "15", 1, bins=["2"]),
            with_one_thread(ALL_COMBOS, [64]),
            BOTH_COLUMNS,
            copies=1,
        )

    # The thesis build in Yu et al.'s setup against the repaired control, and the
    # repaired control against the released artifact, on the HPRD graph and queries
    # they included in their artifact.
    case "validation":
        data = "datasets/HPRD/L15/HPRD-15.txt"
        queries = [f"datasets/HPRD/L15/Q10/Q10-{i}.txt" for i in range(1, 11)]
        runs = with_one_thread(YU_COMBOS, [4, 16, 32])
        add_processes(data, queries, runs, ["yu-setup"], copies=5)
        add_processes(data, queries, runs, ["repaired"], copies=5, binary=REPAIRED)

        # The released artifact aborts on QSplit above 1 thread.
        upstream_runs = [(n, f, t) for n, f, t in runs if t == 1 or "-QorCandi C" in f]
        add_processes(
            data, queries, upstream_runs, ["upstream"], copies=5, binary=UPSTREAM
        )

    case "delivery":
        combos = PILOT_COMBOS | pick(YU_COMBOS, ["pST-eLFTJ"])
        for name, labels in DELIVERY_GRAPHS:
            queries = screened_queries(name, labels, 1)
            for sink in ["count", "globallock", "threadlocal"]:
                runs = with_one_thread(combos)
                # QSplit cannot count.
                if sink == "count":
                    runs = [(n, f, t) for n, f, t in runs if "-QorCandi C" in f]
                add_processes(graph_path(name, labels), queries, runs, [sink], copies=5)

    case "sweep-hprd":
        add_processes(
            graph_path("HPRD", "15"),
            screened_queries("HPRD", "15", 2),
            with_one_thread(YU_COMBOS),
            BOTH_COLUMNS,
            copies=3,
        )

    case "sweep-dblp":
        add_processes(
            graph_path("dblp", "15"),
            screened_queries("dblp", "15", 2),
            with_one_thread(YU_COMBOS),
            ["delivered"],
            copies=3,
        )

    case "sweep-onetbb":
        for name in ["HPRD", "dblp"]:
            add_processes(
                graph_path(name, "15"),
                screened_queries(name, "15", 2),
                cells(ONETBB_COMBOS, THREADS),
                ["delivered"],
                copies=3,
            )

    case "runtime":
        schedules = RUNTIME_SCHEDULES + list(ONETBB_ABLATIONS)
        combos = pick(
            ALL_COMBOS, [f"pEC-{c}-{e}" for c in schedules for e in ["eLFTJ", "eCFL"]]
        )
        for name, labels in MAIN:
            add_processes(
                graph_path(name, labels),
                screened_queries(name, labels, 3),
                with_one_thread(combos),
                ["delivered"],
                copies=5,
            )

    # The slowest screened query in each bin, under three CPU sets: one socket,
    # two sockets, and one socket's cores with their SMT siblings.
    case "placement":
        queries = []
        for b in BINS:
            pool = f"{datasets}/dblp/L15/Q10/bin{b}"
            seconds = {}
            with open(f"{pool}/screened.txt") as f:
                for line in f:
                    if line.strip():
                        query, t = line.split()
                        seconds[query] = float(t)
            slowest = max(seconds, key=seconds.get)
            queries.append(f"{pool}/{slowest}")

        runs = cells(pick(ALL_COMBOS, ["pEC-cBNS-eCFL", "pEC-cOBNS-eCFL"]), [16])
        for cpus in ["0-15", "0-7,16-23", "0-7,32-39"]:
            add_processes(
                graph_path("dblp", "15"),
                queries,
                runs,
                ["delivered"],
                copies=5,
                cpus=cpus,
            )

    case "graphs":
        runs = cells(pick(YU_COMBOS, GRAPH_COMBOS), [16])
        # HPRD at 15 labels comes from the sweep.
        for name, labels in MAIN[1:]:
            add_processes(
                graph_path(name, labels),
                screened_queries(name, labels, 2),
                runs,
                BOTH_COLUMNS,
                copies=3,
            )

    case "labels":
        runs = cells(pick(YU_COMBOS, GRAPH_COMBOS), [16])
        for name in ["HPRD", "dblp"]:
            for labels in ["1", "5", "15", "30", "45", "60"]:
                add_processes(
                    graph_path(name, labels),
                    first_queries(name, labels),
                    runs,
                    BOTH_COLUMNS,
                    copies=3,
                )

    # Size 10 at 15 labels comes from the label experiment.
    case "sizes":
        runs = cells(pick(YU_COMBOS, GRAPH_COMBOS), [16])
        for name in ["HPRD", "dblp"]:
            for size in [20, 30]:
                add_processes(
                    graph_path(name, "15"),
                    first_queries(name, "15", size),
                    runs,
                    BOTH_COLUMNS,
                    copies=3,
                )

    # 30 s against 300 s time limits, to measure how much the 30 s limit biases results.
    # The delivered runs are the cBNS and cOBNS pair the large experiment compares.
    # At 1 label most runs hit the window, so each runs once to keep the 300 s runs cheap.
    case "window":
        graph_runs = cells(pick(YU_COMBOS, GRAPH_COMBOS), [16])
        pair_runs = cells(pick(ALL_COMBOS, ["pEC-cBNS-eCFL", "pEC-cOBNS-eCFL"]), [16])
        for labels, copies in [("15", 3), ("1", 1)]:
            data = graph_path("dblp", labels)
            queries = first_queries("dblp", labels)

            for limit in [30, 300]:
                add_processes(
                    data, queries, graph_runs, ["yu-setup"], copies, time_limit=limit
                )
                add_processes(
                    data, queries, pair_runs, ["delivered"], copies, time_limit=limit
                )

    # Repeats run inside one process, because reloading a large graph for every run
    # costs too much.
    case "large":
        schedules = RUNTIME_SCHEDULES + ["cBNS-cq"]
        runtime = pick(
            ALL_COMBOS, [f"pEC-{c}-{e}" for c in schedules for e in ["eLFTJ", "eCFL"]]
        )
        graph_runs = cells(pick(YU_COMBOS, GRAPH_COMBOS), [16])
        for name, labels in LARGE:
            data = graph_path(name, labels)
            queries = first_queries(name, labels)
            add_processes(
                data,
                queries,
                with_one_thread(runtime),
                ["delivered"],
                copies=1,
                repeats=3,
            )
            add_processes(data, queries, graph_runs, BOTH_COLUMNS, copies=1, repeats=3)

    case _:
        sys.exit(f"unknown experiment {experiment}")

rows = [r for r in rows if int(r["threads"]) <= max_threads]
if not rows:
    sys.exit("no processes: check the datasets directory")

missing = {r["data"] for r in rows if not os.path.exists(r["data"])}
if missing:
    print("warning: missing graphs:", *sorted(missing))

with open(out_path, "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

runs = sum(int(r["repeats"]) for r in rows)
print(f"{out_path}: {len(rows)} processes, {runs} runs")

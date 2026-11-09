#!/usr/bin/env python3
"""Run every process in a plan CSV (from make_plan.py) and write one CSV row per run.

Usage: python3 tools/run_plan.py <plan.csv> <results_dir> [--pair]

Processes run under taskset in passes, shuffled, so identical processes (the plan's
repeats) are spread over the batch. Each process runs its plan row's `runs` times
with one graph load. Kalibera and Jones (ISMM '13) call these two levels executions
and iterations.
Each process writes its driver log and rows to <results_dir>/runs/,
and <results_dir>/results.csv collects the rows of the plan's processes at the end.
Rerunning skips processes that already have their rows, so a killed batch resumes.
Files of processes no longer in the plan stay in runs/ but are left out of results.csv.
"""

import argparse
import csv
import hashlib
import math
import os
import random
import re
import subprocess
import time

FIELDS = {
    "embeddings": r"#Embeddings: (\d+)",
    "enumerate_s": r"Enumerate time \(seconds\): ([\d.eE+-]+)",
    "consolidation_s": r"Consolidation time \(seconds\): ([\d.eE+-]+)",
    "join_s": r"Join time \(seconds\): ([\d.eE+-]+)",
    "load_s": r"Load graphs time \(seconds\): ([\d.eE+-]+)",
    "filter_s": r"Filter vertices time \(seconds\): ([\d.eE+-]+)",
    "memory_mb": r"Memory cost \(MB\): ([\d.eE+-]+)",
    "init_tasks_s": r"InitTasks time: ([\d.eE+-]+)",
    "stored_embeddings": r"stored embeddings: (\d+)",
    "task_num": r"TaskNum: (\d+)",
    "overtime": r"Overtime: (\d+)",
}

# Per-thread lists, summed over threads.
THREAD_FIELDS = {
    "thread_enum_s": r"Thread Enum Time:(.*)",
    "thread_exec_s": r"Thread Execution Time:(.*)",
    "thread_split_s": r"Thread Split Time:(.*)",
    "thread_idle_s": r"Thread Idle Time:(.*)",
}

# Read from the output of /usr/bin/time -v.
PROCESS_FIELDS = {
    "peak_rss_kb": r"Maximum resident set size \(kbytes\): (\d+)",
    "exit_status": r"Exit status: (\d+)",
}

# Physical cores per socket on ICML2. Socket s has CPUs s * 16 to s * 16 + 15.
SOCKET_CORES = 16


def cpu_list(cpus):
    """The CPU numbers in a taskset list, so "0-2,8" gives [0, 1, 2, 8]."""
    out = []
    for part in cpus.split(","):
        lo, _, hi = part.partition("-")
        out += range(int(lo), int(hi or lo) + 1)
    return out


def mhz(cpus):
    """The mean current frequency of the CPUs in a taskset list, in MHz."""
    khz = []
    for c in cpu_list(cpus):
        with open(f"/sys/devices/system/cpu/cpu{c}/cpufreq/scaling_cur_freq") as f:
            khz.append(int(f.read()))
    return sum(khz) / len(khz) / 1000


def load_average():
    with open("/proc/loadavg") as f:
        return f.read().split()[0]


def last_match(pattern, text):
    """The last value `pattern` captures in `text`, or "" if it never matches."""
    found = re.findall(pattern, text)
    return found[-1] if found else ""


def parse(log_text, time_text):
    """Each run in the driver log as a dict, with the /usr/bin/time fields added."""
    process = {k: last_match(p, time_text) for k, p in PROCESS_FIELDS.items()}
    runs = []
    for block in log_text.split("Start the query")[1:]:
        run = {k: last_match(p, block) for k, p in FIELDS.items()}

        for k, p in THREAD_FIELDS.items():
            values = last_match(p, block)
            run[k] = ""
            if values.strip():
                run[k] = sum(float(x) for x in values.split(","))

        times = [run["enumerate_s"], run["consolidation_s"], run["join_s"]]
        seconds = sum(float(t or 0) for t in times)
        run["eps"] = ""
        if run["embeddings"] and seconds:
            run["eps"] = round(int(run["embeddings"]) / seconds)

        runs.append({**run, **process})
    return runs


parser = argparse.ArgumentParser()
parser.add_argument("plan")
parser.add_argument("results_dir")
parser.add_argument(
    "--pair",
    action="store_true",
    help="run two processes at once, one per socket, when each fits on one socket",
)
parser.add_argument("--seed", type=int, default=0, help="shuffle seed")
parser.add_argument(
    "--overwrite",
    action="store_true",
    help="delete the results of every process in the plan first, so all of them rerun",
)
args = parser.parse_args()

# A process's id is a hash of its plan row plus a number among identical rows, so
# regenerating the plan
# keeps resume correct.
with open(args.plan) as f:
    plan = list(csv.DictReader(f))
processes = {}
for row in plan:
    key = hashlib.sha1("|".join(row.values()).encode()).hexdigest()[:12]
    processes[key] = processes.get(key, 0) + 1
    row["id"] = f"{key}-{processes[key]}"

binary_hash = {}
for b in {r["binary"] for r in plan}:
    with open(b, "rb") as f:
        binary_hash[b] = hashlib.sha1(f.read()).hexdigest()[:12]

run_dir = os.path.join(args.results_dir, "runs")
os.makedirs(run_dir, exist_ok=True)


def run_path(row, extension):
    """Where a process's log or rows go, named so `ls runs/` shows what each one is.

    The id at the end keeps the name unique when two processes differ only in a column
    the name leaves out, or are identical processes.
    """
    graph = os.path.basename(row["data"]).removesuffix(".txt")
    query_dir, query_file = os.path.split(row["query"])
    query = os.path.basename(query_dir) + "-" + query_file.removesuffix(".txt")
    flags = row["flags"].split()
    time_limit = flags[flags.index("-time_limit") + 1]

    name = f"{graph}_{query}_{row['combination']}_{row['delivery']}"
    name += f"_t{row['threads']}_{time_limit}s"
    if row["cpus"]:
        name += f"_cpus{row['cpus']}"
    return os.path.join(run_dir, f"{name}_{row['id']}.{extension}")


if args.overwrite:
    for row in plan:
        if os.path.exists(run_path(row, "csv")):
            os.remove(run_path(row, "csv"))

# A process runs again from the start unless every run recorded an embedding count,
# so a driver that crashed or was killed partway reruns on the next resume.
todo = []
for row in plan:
    finished = 0
    if os.path.exists(run_path(row, "csv")):
        with open(run_path(row, "csv")) as f:
            for r in csv.DictReader(f):
                if r["embeddings"]:
                    finished += 1
    if finished < int(row["runs"]):
        todo.append(row)

# With --pair, a process shares the machine with one other only if it has no CPU set of
# its own and fits on one socket.
alone_ids = set()
for row in todo:
    if not args.pair or row["cpus"] or int(row["threads"]) > SOCKET_CORES:
        alone_ids.add(row["id"])

# Process k of n identical processes runs in pass ceil(k * max_processes / n),
# so a batch stopped after any pass has about the same share of every cell's repeats.
max_processes = max(processes.values())
passes = {}
for row in todo:
    key, number = row["id"].rsplit("-", 1)
    pass_number = math.ceil(int(number) * max_processes / processes[key])
    passes.setdefault(pass_number, []).append(row)

# Each pass is shuffled. With --pair, its pairable processes go first,
# so the ones that run alone don't keep draining the other socket.
rng = random.Random(args.seed)
order = []
for p in sorted(passes):
    pairable = [r for r in passes[p] if r["id"] not in alone_ids]
    alone = [r for r in passes[p] if r["id"] in alone_ids]
    rng.shuffle(pairable)
    rng.shuffle(alone)
    order += pairable + alone
print(
    f"{len(todo)} of {len(plan)} processes to run, in {len(passes)} passes", flush=True
)

columns = [
    *plan[0],
    *["binary_sha1", "run", "cpus_used", "slot", "t_start", "t_end"],
    *["load_start", "load_end", "mhz_run"],
    *FIELDS,
    *THREAD_FIELDS,
    "eps",
    *PROCESS_FIELDS,
]

# Socket number -> the process running on it.
running = {}


def start(row, socket, slot):
    """Start a process on the socket's CPUs, or on the CPU set the plan gives it.

    `slot` is "alone" or "pair", and goes in the process's rows.
    """
    threads = int(row["threads"])
    first = SOCKET_CORES * socket
    cpus = row["cpus"] or f"{first}-{first + threads - 1}"

    # The driver appends to its log, so a resumed process must not see an old one.
    log = run_path(row, "log")
    if os.path.exists(log):
        os.remove(log)

    thread_counts = ",".join([row["threads"]] * int(row["runs"]))
    command = ["taskset", "-c", cpus, "/usr/bin/time", "-v", row["binary"]]
    command += ["-d", row["data"], "-q", row["query"], *row["flags"].split()]
    command += ["-threadnums", thread_counts, "-num", "MAX", "-OutputFile", log]

    process = subprocess.Popen(
        command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True
    )
    running[socket] = {
        "process": process,
        "row": row,
        "cpus": cpus,
        "slot": slot,
        "log": log,
        "t_start": time.time(),
        "load_start": load_average(),
        "mhz": [],
    }


def finish(socket):
    """Collect the process on the socket, report it if it failed, and write its rows."""
    job = running.pop(socket)
    time_text = job["process"].communicate()[1]
    log_text = ""
    if os.path.exists(job["log"]):
        with open(job["log"]) as f:
            log_text = f.read()

    runs = parse(log_text, time_text)
    row = job["row"]
    failed = job["process"].returncode != 0 or len(runs) < int(row["runs"])
    for run in runs:
        if not run["embeddings"]:
            failed = True
    if failed:
        name = f"{row['experiment']} {row['combination']} t={row['threads']}"
        print(f"FAILED: {name}, see {job['log']}", flush=True)

    # A process with no runs still gets one row, so the failure shows in the results
    # and the next resume runs it again.
    if not runs:
        runs = [{}]

    mhz_run = ""
    if job["mhz"]:
        mhz_run = round(sum(job["mhz"]) / len(job["mhz"]))
    common = {
        "binary_sha1": binary_hash[row["binary"]],
        "cpus_used": job["cpus"],
        "slot": job["slot"],
        "t_start": round(job["t_start"], 2),
        "t_end": round(time.time(), 2),
        "load_start": job["load_start"],
        "load_end": load_average(),
        "mhz_run": mhz_run,
    }

    # First write to a temporary name, so a killed batch doesn't leave a partial file.
    path = run_path(row, "csv")
    with open(path + ".tmp", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=columns)
        writer.writeheader()

        # Only GSplit stores whole embeddings.
        # QSplit stores its units' partial matches, so the two counts differ.
        check_stored = "-QorCandi C" in row["flags"] and "-mode match" in row["flags"]

        for number, run in enumerate(runs, 1):
            finished = run.get("overtime") == "0"
            stored = run.get("stored_embeddings")
            if check_stored and finished and stored != run.get("embeddings"):
                print(f"STORED != COUNTED: {row['id']} run {number}", flush=True)
            writer.writerow({**row, **run, **common, "run": number})
    os.replace(path + ".tmp", path)


def wait_for_one():
    """Sample each running process's CPU frequency until one exits, then record it."""
    while True:
        for socket, job in list(running.items()):
            if job["process"].poll() is not None:
                finish(socket)
                return
            job["mhz"].append(mhz(job["cpus"]))
        time.sleep(0.05)


# A process that runs alone waits for the machine to empty, then holds it until done.
# A paired process waits for a free socket.
for n, row in enumerate(order, 1):
    if row["id"] in alone_ids:
        while running:
            wait_for_one()
        start(row, 0, "alone")
        wait_for_one()
    else:
        if len(running) == 2:
            wait_for_one()
        socket = 0
        if 0 in running:
            socket = 1
        start(row, socket, "pair")

    if n % 100 == 0:
        print(f"{n}/{len(todo)} started", flush=True)

while running:
    wait_for_one()

results = []
for row in plan:
    if os.path.exists(run_path(row, "csv")):
        with open(run_path(row, "csv")) as f:
            results += list(csv.DictReader(f))

results_path = os.path.join(args.results_dir, "results.csv")
with open(results_path, "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=columns)
    writer.writeheader()
    writer.writerows(results)

# Completed runs of one query should agree on its embeddings, whatever the combination.
counts = {}
for r in results:
    if r["embeddings"] and r["overtime"] == "0":
        counts.setdefault((r["data"], r["query"]), set()).add(r["embeddings"])
for (data, query), found in counts.items():
    if len(found) > 1:
        print(f"COUNTS DISAGREE: {data} {query}: {sorted(found)}")
print(f"done: {results_path}")

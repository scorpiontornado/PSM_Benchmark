#!/usr/bin/env python3
"""Run every process in a plan CSV (from make_plan.py) and write one CSV row per run.

Usage: python3 tools/run_plan.py <plan.csv> <results_dir> [--pair] [--perf]

Processes run under taskset in passes, shuffled, so identical processes (the plan's
repeats) are spread over the batch. Each process runs its plan row's `runs` times
with one graph load. Kalibera and Jones (ISMM '13) call these two levels executions
and iterations.
Each process writes its driver log, rows, stderr (ending with /usr/bin/time -v's
report) and, with --perf, perf stat's output to <results_dir>/runs/, and
<results_dir>/results.csv collects the rows of the plan's processes at the end.
Rerunning skips processes that already have their rows, so a killed batch resumes.
After a run times out, the identical processes of its cell that haven't run are skipped.
Files of processes no longer in the plan stay in runs/ but are left out of results.csv.
Each batch appends the machine's state at its start to <results_dir>/machine.txt.
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
    "init_alloc_s": r"InitTasks allocate time: ([\d.eE+-]+)",
    "init_task_num": r"InitTasks allocate time: .* to make (\d+) tasks",
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

# Read from the output of /usr/bin/time -v. These and PERF_FIELDS cover the whole
# process, including loading and filtering, and repeat on each of its runs' rows.
PROCESS_FIELDS = {
    "peak_rss_kb": r"Maximum resident set size \(kbytes\): (\d+)",
    "exit_status": r"Exit status: (\d+)",
    "user_s": r"User time \(seconds\): ([\d.]+)",
    "sys_s": r"System time \(seconds\): ([\d.]+)",
    "minor_faults": r"Minor \(reclaiming a frame\) page faults: (\d+)",
    "voluntary_switches": r"Voluntary context switches: (\d+)",
    "involuntary_switches": r"Involuntary context switches: (\d+)",
}

# Read from the CSV output of perf stat -x, (value, unit, event name, ...), with
# --perf. Clock speed is cycles over task-clock. On Intel, cache-references and
# cache-misses count the last-level cache, and node-loads and node-load-misses
# count reads that missed it and were served by this socket's memory or the other's.
# An event the CPU doesn't support is left blank.
PERF_FIELDS = {
    "perf_task_clock_ms": r"(?m)^([\d.]+),msec,task-clock,",
    "perf_cycles": r"(?m)^(\d+),,cycles,",
    "perf_instructions": r"(?m)^(\d+),,instructions,",
    "perf_llc_references": r"(?m)^(\d+),,cache-references,",
    "perf_llc_misses": r"(?m)^(\d+),,cache-misses,",
    "perf_node_loads": r"(?m)^(\d+),,node-loads,",
    "perf_node_load_misses": r"(?m)^(\d+),,node-load-misses,",
}

# Physical cores per socket on ICML2. Socket s has CPUs s * 16 to s * 16 + 15.
SOCKET_CORES = 16

# Moves to the start of the terminal line and erases it, so the progress line can be
# overwritten by the next one or by a message that should stay on screen.
CLEAR_LINE = "\r\033[K"


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
    "--perf",
    action="store_true",
    help="also count each process's cycles, instructions, last-level cache "
    "references and misses, and local and remote memory reads with perf stat",
)
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


def cell_of(row):
    """The id shared by a process and its identical repeats."""
    return row["id"].rsplit("-", 1)[0]


# Once any repeat of a cell times out, its remaining repeats are skipped and the cell
# counts as timed out, so time-limited cells cost one run instead of every repeat.
timed_out_cells = set()

# A process runs again from the start unless every run recorded an embedding count,
# so a driver that crashed or was killed partway reruns on the next resume.
unfinished = []
for row in plan:
    finished = 0
    if os.path.exists(run_path(row, "csv")):
        with open(run_path(row, "csv")) as f:
            for r in csv.DictReader(f):
                if r["embeddings"]:
                    finished += 1
                if r["overtime"] == "1":
                    timed_out_cells.add(cell_of(row))
    if finished < int(row["runs"]):
        unfinished.append(row)

todo = []
for row in unfinished:
    if cell_of(row) not in timed_out_cells:
        todo.append(row)
skipped = len(unfinished) - len(todo)

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
# Number of processes started when a pass ends -> that pass's number.
pass_ends = {}
for p in sorted(passes):
    pairable = [r for r in passes[p] if r["id"] not in alone_ids]
    alone = [r for r in passes[p] if r["id"] in alone_ids]
    rng.shuffle(pairable)
    rng.shuffle(alone)
    order += pairable + alone
    pass_ends[len(order)] = p
print(
    f"{len(todo)} of {len(plan)} processes to run, in {len(passes)} passes; "
    f"{skipped} skipped because a repeat timed out",
    flush=True,
)

columns = [
    *plan[0],
    *["binary_sha1", "run", "cpus_used", "slot", "t_start", "t_end"],
    *["load_start", "load_end", "mhz_run"],
    *FIELDS,
    *THREAD_FIELDS,
    "eps",
    *PROCESS_FIELDS,
    *PERF_FIELDS,
]

# Socket number -> the process running on it.
running = {}


def start(row, socket, slot):
    """Start a process on the CPUs its slot gets, or on the CPU set the plan gives it.

    `slot` is "alone" or "pair", and goes in the process's rows.
    """
    threads = int(row["threads"])

    # A process running alone takes the highest-numbered physical cores, away from
    # CPU 0 and the low CPUs other users are likely to pin to. Linux numbers every
    # core's second hardware thread after all the first ones, so the physical cores
    # are the first half of the CPUs.
    if slot == "alone":
        cores = os.cpu_count() // 2
        default_cpus = f"{cores - threads}-{cores - 1}"
    else:
        first = SOCKET_CORES * socket
        default_cpus = f"{first}-{first + threads - 1}"
    cpus = row["cpus"] or default_cpus

    # The driver appends to its log, so a resumed process must not see an old one.
    log = run_path(row, "log")
    if os.path.exists(log):
        os.remove(log)

    thread_counts = ",".join([row["threads"]] * int(row["runs"]))
    command = ["taskset", "-c", cpus]
    if args.perf:
        events = "task-clock,cycles,instructions,cache-references,cache-misses"
        events += ",node-loads,node-load-misses"
        command += ["perf", "stat", "-x", ",", "-e", events]
        command += ["-o", run_path(row, "perf"), "--"]
    command += ["/usr/bin/time", "-v", row["binary"]]
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
    with open(run_path(job["row"], "stderr"), "w") as f:
        f.write(time_text)
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
        print(f"{CLEAR_LINE}FAILED: {name}, see {job['log']}", flush=True)

    # A process with no runs still gets one row, so the failure shows in the results
    # and the next resume runs it again.
    if not runs:
        runs = [{}]

    mhz_run = ""
    if job["mhz"]:
        mhz_run = round(sum(job["mhz"]) / len(job["mhz"]))

    perf_text = ""
    if args.perf and os.path.exists(run_path(row, "perf")):
        with open(run_path(row, "perf")) as f:
            perf_text = f.read()

    common = {
        **{k: last_match(p, perf_text) for k, p in PERF_FIELDS.items()},
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
            if run.get("overtime") == "1":
                timed_out_cells.add(cell_of(row))
            finished = run.get("overtime") == "0"
            stored = run.get("stored_embeddings")
            if check_stored and finished and stored != run.get("embeddings"):
                message = f"STORED != COUNTED: {row['id']} run {number}"
                print(CLEAR_LINE + message, flush=True)
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


# Record the machine's state at the start of every batch, for the write-up.
with open(os.path.join(args.results_dir, "machine.txt"), "a") as f:
    commands = ["date", "uname -r", "uptime", "who", "vmstat 1 5", "lscpu", "free -g"]
    # Linux usually maps memory 4 KiB at a time, and the first write to each
    # page triggers a page fault, so storing results costs ~250,000 page faults per GB.
    # With transparent huge pages set to "always", it can map large allocations
    # 2 MiB at a time, so big result blocks cause far fewer faults.
    commands.append("cat /sys/kernel/mm/transparent_hugepage/enabled")
    for command in commands:
        f.write(f"$ {command}\n")
        f.flush()
        subprocess.run(command.split(), stdout=f, stderr=subprocess.STDOUT, check=False)

# A process that runs alone waits for the machine to empty, then holds it until done.
# A paired process waits for a free socket.
batch_start = time.time()
for n, row in enumerate(order, 1):
    if cell_of(row) in timed_out_cells:
        skipped += 1
    elif row["id"] in alone_ids:
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

    # The progress line overwrites itself. The line at the end of a pass stays.
    minutes = (time.time() - batch_start) / 60
    left = minutes / n * (len(todo) - n)
    progress = (
        f"{n}/{len(todo)} started, {minutes:.0f} min so far, about {left:.0f} left"
    )
    print(CLEAR_LINE + progress, end="", flush=True)
    if n in pass_ends:
        done = f"pass {pass_ends[n]} of {len(passes)} done, {minutes:.0f} min so far"
        print(CLEAR_LINE + done, flush=True)

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
minutes = (time.time() - batch_start) / 60
print(f"{skipped} processes skipped because a repeat of theirs timed out")
print(f"done in {minutes:.0f} min: {results_path}")

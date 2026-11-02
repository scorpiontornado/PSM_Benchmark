#!/usr/bin/env python3
"""Convert an LDBC LSQB "projected-fk" CSV directory into a driver graph file.

Usage: python3 tools/lsqb_to_graph.py <social-network-sfX-projected-fk/> <out.txt>

The graph is made simple and undirected, with all edge types merged, and each vertex
type becomes one label.
"""

import argparse
import glob
import os
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("csv_dir")
parser.add_argument("out")
args = parser.parse_args()

files = sorted(glob.glob(os.path.join(args.csv_dir, "*.csv")))
types = [os.path.basename(p)[:-4] for p in files if "_" not in os.path.basename(p)]

index = {}
labels = []
for label, name in enumerate(types):
    ids = index[name] = {}
    with open(os.path.join(args.csv_dir, name + ".csv")) as f:
        f.readline()
        for line in f:
            ids[line.rstrip("\n")] = len(labels)
            labels.append(label)
    print(f"label {label} {name}: {len(ids)} vertices")

# Temporary files go to $TMPDIR: on a full disk, fetch_datasets.sh points it at /dev/shm.
raw = os.path.join(tempfile.gettempdir(), "lsqb.edges.tmp")
with open(raw, "w") as out:
    for path in files:
        parts = os.path.basename(path)[:-4].split("_")
        if len(parts) == 1:
            continue
        src, dst = index[parts[0]], index[parts[-1]]
        with open(path) as f:
            f.readline()
            for line in f:
                a, b = line.rstrip("\n").split("|")
                a, b = src[a], dst[b]
                if a < b:
                    out.write(f"e {a} {b}\n")
                elif b < a:
                    out.write(f"e {b} {a}\n")
del index

unique = os.path.join(tempfile.gettempdir(), "lsqb.unique.tmp")
tmp_dir = tempfile.gettempdir()
subprocess.run(
    ["sort", "-u", "-S", "4G", "-T", tmp_dir, "-o", unique, raw],
    env=dict(os.environ, LC_ALL="C"),
    check=True,
)
os.remove(raw)

degree = [0] * len(labels)
num_edges = 0
with open(unique) as f:
    for line in f:
        _, a, b = line.split()
        degree[int(a)] += 1
        degree[int(b)] += 1
        num_edges += 1

with open(args.out, "w") as out:
    out.write(f"t {len(labels)} {num_edges}\n")
    out.writelines(f"v {i} {labels[i]} {degree[i]}\n" for i in range(len(labels)))
    with open(unique) as f:
        shutil.copyfileobj(f, out)
os.remove(unique)

print(
    f"{args.out}: {len(labels)} vertices ({degree.count(0)} isolated), {num_edges} edges, {len(types)} labels"
)

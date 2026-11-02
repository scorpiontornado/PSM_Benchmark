#!/usr/bin/env python3
"""Convert an edge list into a driver graph file with uniformly random labels.

Usage: python3 tools/edgelist_to_graph.py <edges[.gz]> <num_labels> <out.txt> [--seed N]

The graph is made simple and undirected, and vertices are renumbered from 0.

Supported formats include SNAP and CSV, optionally gzipped.
"""

import argparse
import gzip
import random

parser = argparse.ArgumentParser()
parser.add_argument("edges", help="edge list (SNAP/CSV), optionally gzipped")
parser.add_argument("num_labels", type=int)
parser.add_argument("out")
parser.add_argument("--seed", type=int, default=0, help="label RNG seed (default 0)")
args = parser.parse_args()

edges = set()
with (gzip.open if args.edges.endswith(".gz") else open)(args.edges, "rt") as f:
    for line in f:
        parts = line.replace(",", " ").split()
        if len(parts) < 2 or not parts[0].isdigit():
            continue
        a, b = int(parts[0]), int(parts[1])
        if a != b:
            edges.add((min(a, b), max(a, b)))

index = {}
for a, b in sorted(edges):
    index.setdefault(a, len(index))
    index.setdefault(b, len(index))

edges = sorted((min(index[a], index[b]), max(index[a], index[b])) for a, b in edges)
degree = [0] * len(index)

for a, b in edges:
    degree[a] += 1
    degree[b] += 1

rng = random.Random(args.seed)
with open(args.out, "w") as out:
    out.write(f"t {len(index)} {len(edges)}\n")
    out.writelines(
        f"v {i} {rng.randrange(args.num_labels)} {degree[i]}\n"
        for i in range(len(index))
    )
    for a, b in edges:
        out.write(f"e {a} {b}\n")

print(
    f"{args.out}: {len(index)} vertices, {len(edges)} edges, {args.num_labels} labels"
)

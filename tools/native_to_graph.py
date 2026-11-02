#!/usr/bin/env python3
"""Convert a graph that already has vertex labels into a driver graph file.

Usage: python3 tools/native_to_graph.py <in.gfu|in.graph> <out.txt>

The graph is made simple and undirected, isolated vertices are dropped, and vertices
and labels are renumbered from 0.

Supported formats are VEQ's .gfu and t/v/e text.
"""

import argparse

parser = argparse.ArgumentParser()
parser.add_argument("graph")
parser.add_argument("out")
args = parser.parse_args()

labels = []
pairs = []
with open(args.graph) as f:
    if args.graph.endswith(".gfu"):
        f.readline()
        labels = [int(f.readline()) for _ in range(int(f.readline()))]
        pairs = [f.readline().split()[:2] for _ in range(int(f.readline()))]
    else:
        for line in f:
            parts = line.split()
            if parts and parts[0] == "v":
                labels.append(int(parts[2]))
            elif parts and parts[0] == "e":
                pairs.append(parts[1:3])

edges = set()
for a, b in pairs:
    a, b = int(a), int(b)
    if a != b:
        edges.add((min(a, b), max(a, b)))

index = {}
for a, b in sorted(edges):
    index.setdefault(a, len(index))
    index.setdefault(b, len(index))

edges = sorted((min(index[a], index[b]), max(index[a], index[b])) for a, b in edges)
kept = list(index)
compact = {label: i for i, label in enumerate(sorted({labels[v] for v in kept}))}
degree = [0] * len(kept)

for a, b in edges:
    degree[a] += 1
    degree[b] += 1

with open(args.out, "w") as out:
    out.write(f"t {len(kept)} {len(edges)}\n")
    out.writelines(
        f"v {i} {compact[labels[v]]} {degree[i]}\n" for i, v in enumerate(kept)
    )
    for a, b in edges:
        out.write(f"e {a} {b}\n")

dropped = len(labels) - len(kept)
print(
    f"{args.out}: {len(kept)} vertices ({dropped} isolated dropped), "
    f"{len(edges)} edges, {len(compact)} labels"
)

#!/usr/bin/env python3
"""Generate random-walk query graphs from a driver graph file.

Usage: python3 tools/gen_queries.py <graph.txt> <size> <count> <out_dir> [--seed N] [--min-degree D] [--max-degree D]

Each query is the subgraph induced by the first <size> distinct vertices of a random walk.
With the degree bounds, only queries whose average degree 2|E|/|V| is in [min, max) are kept.

See: Yu et al. (2026), Section 6.1; Sun and Luo (2020).
"""

import argparse
import os
import random
from array import array

parser = argparse.ArgumentParser()
parser.add_argument("graph")
parser.add_argument("size", type=int, help="vertices per query")
parser.add_argument("count", type=int, help="number of queries")
parser.add_argument("out_dir")
parser.add_argument("--seed", type=int, default=0, help="walk RNG seed (default 0)")
parser.add_argument(
    "--min-degree",
    type=float,
    default=0,
    help="keep queries with average degree >= this",
)
parser.add_argument(
    "--max-degree",
    type=float,
    default=float("inf"),
    help="keep queries with average degree < this",
)
args = parser.parse_args()

# Same CSR layout as graph.cpp: offsets from the v-line degrees, then edges fill in.
with open(args.graph) as f:
    _, n, m = f.readline().split()
    labels = array("I", bytes(4 * int(n)))
    offsets = array("Q", bytes(8 * (int(n) + 1)))
    neighbors = array("I", bytes(8 * int(m)))
    filled = array("Q", bytes(8 * int(n)))
    for line in f:
        kind, a, b, *rest = line.split()
        a, b = int(a), int(b)
        if kind == "v":
            labels[a] = b
            offsets[a + 1] = offsets[a] + int(rest[0])
            filled[a] = offsets[a]
        else:
            neighbors[filled[a]] = b
            neighbors[filled[b]] = a
            filled[a] += 1
            filled[b] += 1


def walk(rng):
    v = rng.randrange(len(labels))
    visited = [v]
    for _ in range(100 * args.size):
        if len(visited) == args.size or offsets[v] == offsets[v + 1]:
            break
        v = neighbors[rng.randrange(offsets[v], offsets[v + 1])]
        if v not in visited:
            visited.append(v)
    return visited if len(visited) == args.size else None


rng = random.Random(args.seed)
os.makedirs(args.out_dir, exist_ok=True)
seen = set()
i = 0
walks = 0

# A sparse bin may be nearly unreachable for dense graphs, so cap the number of iterations.
while i < args.count and walks < 1_000_000:
    walks += 1
    vertices = walk(rng)
    if vertices is None or frozenset(vertices) in seen:
        continue
    seen.add(frozenset(vertices))

    local = {v: j for j, v in enumerate(vertices)}
    edges = [
        (local[v], local[w])
        for v in vertices
        for w in neighbors[offsets[v] : offsets[v + 1]]
        if w in local and local[v] < local[w]
    ]

    if not args.min_degree <= 2 * len(edges) / args.size < args.max_degree:
        continue

    i += 1
    degree = [0] * args.size
    for a, b in edges:
        degree[a] += 1
        degree[b] += 1

    name = f"Q{args.size}-{i}.txt"
    with open(os.path.join(args.out_dir, name), "w") as out:
        out.write(f"t {args.size} {len(edges)}\n")
        out.writelines(
            f"v {j} {labels[v]} {degree[j]}\n" for j, v in enumerate(vertices)
        )
        for a, b in edges:
            out.write(f"e {a} {b}\n")
    print(
        f"{name}: {len(edges)} edges, average degree {2 * len(edges) / args.size:.2f}"
    )

print(f"{args.out_dir}: {i} queries from {walks} walks")

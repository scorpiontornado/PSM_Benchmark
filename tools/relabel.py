#!/usr/bin/env python3
"""Rewrite the vertex labels of a graph file, keeping the text format.

    python3 tools/relabel.py <in.txt> <out.txt> <num_labels> [--seed N]

The driver only reads the text format (Graph::loadGraphFromFile), so the
ReassignLabel tool's binary output cannot be used with it. Labels are drawn
uniformly, matching how the paper assigns them; real skewed label
distributions need a pre-labelled dataset instead.
"""

import argparse
import random


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("num_labels", type=int)
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    vertices = 0

    with open(args.input) as fin, open(args.output, "w") as fout:
        for line in fin:
            if line.startswith("v "):
                # v <id> <label> <degree>
                parts = line.split()
                parts[2] = str(rng.randrange(args.num_labels))
                fout.write(" ".join(parts) + "\n")
                vertices += 1
            else:
                fout.write(line)

    print(
        f"relabelled {vertices} vertices to {args.num_labels} labels -> {args.output}"
    )


if __name__ == "__main__":
    main()

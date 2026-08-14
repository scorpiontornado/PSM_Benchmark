#!/usr/bin/env python3
"""Reduce the label alphabet of a data graph and its query graphs.

    python3 tools/relabel.py <num_labels> <out_dir> <graph.txt> [query.txt ...]

Every label becomes `label % num_labels`, so labels are merged rather than
reassigned. Two things follow from that:

- It can only reduce the label count, never raise it.
- Merged labels are as common as the sum of their parts, so the label
  distribution changes shape and does not stay uniform.

Pass the data graph and its query graphs in one call. They must share the
mapping: a query carrying a label the data graph does not have crashes the
benchmark with an out-of-bounds read.
"""

import argparse
import os


def relabel(path, out_dir, num_labels):
    out_path = os.path.join(out_dir, os.path.basename(path))
    labels = set()

    with open(path) as fin, open(out_path, "w") as fout:
        for line in fin:
            if line.startswith("v "):
                # v <id> <label> <degree>
                parts = line.split()
                parts[2] = str(int(parts[2]) % num_labels)
                labels.add(parts[2])
                fout.write(" ".join(parts) + "\n")
            else:
                fout.write(line)

    return out_path, len(labels)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("num_labels", type=int)
    parser.add_argument("out_dir")
    parser.add_argument("files", nargs="+")
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    for path in args.files:
        out_path, distinct = relabel(path, args.out_dir, args.num_labels)
        print(f"{out_path}: {distinct} distinct labels")


if __name__ == "__main__":
    main()

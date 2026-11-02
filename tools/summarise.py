#!/usr/bin/env python3
"""Print each config's EPS and embedding counts from a compare.py CSV.

Usage: python3 tools/summarise.py <compare.csv>

EPS is printed in millions of embeddings per second, as the median, min and max over
the config's runs. Embedding counts are printed as-is.

For n runs, [min, max] is a nonparametric confidence interval for the median with
coverage 1 - 2 * 0.5**n (96.9% at n = 6). This follows recommendations by
Hoefler and Belli (2015): using nonparametric intervals for timings that aren't
normally distributed.
"""

import csv
import sys
from statistics import median

runs = {}
with open(sys.argv[1]) as f:
    for row in csv.DictReader(f):
        runs.setdefault((row["config"], int(row["threads"])), []).append(row)

print("config threads n median_eps_M min_eps_M max_eps_M embeddings")
for (config, threads), rows in sorted(runs.items()):
    eps = [float(r["eps"]) for r in rows if r["eps"]] or [0.0]
    mid = round(median(eps) / 1e6, 1)
    low = round(min(eps) / 1e6, 1)
    high = round(max(eps) / 1e6, 1)

    counts = ",".join(sorted({r["embeddings"] for r in rows}))

    print(config, threads, len(eps), mid, low, high, counts)

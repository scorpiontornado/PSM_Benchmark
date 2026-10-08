"""Print median, min and max EPS and the embedding counts per config from a compare.py CSV.

With n runs, [min, max] is a nonparametric confidence interval for the median with
coverage 1 - 2 * 0.5**n (96.9% at n = 6). Hoefler and Belli (2015) recommend
nonparametric intervals like this for timings that are not normally distributed.

Usage: python3 tools/summarise.py <compare.csv>
"""

import csv
import sys
from statistics import median

runs = {}
for row in csv.DictReader(open(sys.argv[1])):
    runs.setdefault((row["config"], int(row["threads"])), []).append(row)

print("config threads n median_eps_M min_eps_M max_eps_M embeddings")
for (config, threads), rows in sorted(runs.items()):
    eps = [float(r["eps"]) for r in rows if r["eps"]] or [0]
    counts = ",".join(sorted({r["embeddings"] for r in rows}))
    print(config, threads, len(eps), *(round(x / 1e6, 1) for x in (median(eps), min(eps), max(eps))), counts)

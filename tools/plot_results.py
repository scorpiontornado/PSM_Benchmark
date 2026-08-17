"""Draw the three presentation charts from a compare.py results CSV.

Usage: python3 tools/plot_results.py <compare.csv> <outdir>

Writes eps_vs_threads, consolidation and gsplit_vs_qsplit as both PNG and SVG.
Expects the CSV to hold the four configs c_count, c_threadlocal, c_globallock
and q_threadlocal at thread counts 1, 2, 4 and 8.
"""

import csv
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

BLUE = "#2a78d6"
ORANGE = "#eb6834"
AQUA = "#1baf7a"
INK = "#0b0b0b"
MUTED = "#52514e"
GRID = "#dedddb"

THREADS = [1, 2, 4, 8]


def load(path):
    """Return {config: {threads: row}} with the numeric fields already parsed."""
    table = {}
    with open(path) as f:
        for row in csv.DictReader(f):
            row["eps"] = float(row["eps"]) / 1e6
            row["enumerate_s"] = float(row["enumerate_s"])
            row["consolidation_s"] = float(row["consolidation_s"])
            table.setdefault(row["config"], {})[int(row["threads"])] = row
    return table


def style(ax):
    ax.set_facecolor("white")
    ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
    ax.tick_params(colors=MUTED, length=0)


def save(fig, outdir, name):
    for ext in ("png", "svg"):
        fig.savefig(f"{outdir}/{name}.{ext}", dpi=200, bbox_inches="tight",
                    facecolor="white")
    plt.close(fig)
    print(f"wrote {outdir}/{name}.png and .svg")


def eps_vs_threads(table, outdir):
    series = [
        ("c_count", "Counted, not delivered", BLUE),
        ("c_threadlocal", "Delivered: thread-local sink", AQUA),
        ("c_globallock", "Delivered: global lock", ORANGE),
    ]
    fig, ax = plt.subplots(figsize=(9, 5))
    style(ax)
    for config, label, colour in series:
        y = [table[config][t]["eps"] for t in THREADS]
        ax.plot(THREADS, y, color=colour, linewidth=2.5, marker="o",
                markersize=8, markeredgecolor="white", markeredgewidth=1.5,
                label=label, zorder=3)
        ax.annotate(f"{y[-1]:.1f}", (THREADS[-1], y[-1]), textcoords="offset points",
                    xytext=(10, 0), va="center", color=INK, fontsize=11,
                    fontweight="bold")
    ax.set_xscale("log", base=2)
    ax.set_xticks(THREADS)
    ax.set_xticklabels([str(t) for t in THREADS])
    ax.set_xlim(0.9, 10.5)
    ax.set_ylim(0, 140)
    ax.set_xlabel("Threads", color=MUTED, fontsize=11)
    ax.set_ylabel("Million embeddings per second", color=MUTED, fontsize=11)
    ax.set_title("Counting scales. Delivering results does not.",
                 color=INK, fontsize=14, fontweight="bold", loc="left", pad=14)
    ax.legend(frameon=False, loc="upper left", fontsize=11, labelcolor=INK)
    save(fig, outdir, "eps_vs_threads")


def consolidation(table, outdir):
    enum = [table["c_threadlocal"][t]["enumerate_s"] for t in THREADS]
    cons = [table["c_threadlocal"][t]["consolidation_s"] for t in THREADS]
    x = list(range(len(THREADS)))

    fig, ax = plt.subplots(figsize=(9, 5))
    style(ax)
    ax.bar(x, enum, width=0.55, color=BLUE, label="Enumerate (parallel)", zorder=3)
    ax.bar(x, cons, width=0.55, bottom=[e + 0.04 for e in enum], color=ORANGE,
           label="Consolidate (serial)", zorder=3)
    for i, (e, c) in enumerate(zip(enum, cons)):
        ax.annotate(f"{c / (e + c):.0%} serial", (i, e + c + 0.25), ha="center",
                    color=INK, fontsize=11, fontweight="bold")
    ax.set_xticks(x)
    ax.set_xticklabels([str(t) for t in THREADS])
    ax.set_ylim(0, 14)
    ax.set_xlabel("Threads", color=MUTED, fontsize=11)
    ax.set_ylabel("Seconds", color=MUTED, fontsize=11)
    ax.set_title("Thread-local buffering defers the cost, it does not remove it",
                 color=INK, fontsize=14, fontweight="bold", loc="left", pad=14)
    ax.legend(frameon=False, loc="upper right", fontsize=11, labelcolor=INK)
    save(fig, outdir, "consolidation")


def gsplit_vs_qsplit(table, outdir):
    g = [table["c_threadlocal"][t]["eps"] for t in THREADS]
    q = [table["q_threadlocal"][t]["eps"] for t in THREADS]
    x = list(range(len(THREADS)))
    w = 0.36

    fig, ax = plt.subplots(figsize=(9, 5))
    style(ax)
    ax.bar([i - w / 2 - 0.01 for i in x], g, width=w, color=BLUE, label="GSplit",
           zorder=3)
    ax.bar([i + w / 2 + 0.01 for i in x], q, width=w, color=AQUA, label="QSplit",
           zorder=3)
    for i, (a, b) in enumerate(zip(g, q)):
        ax.annotate(f"{a:.1f}", (i - w / 2, a + 0.8), ha="center", color=INK,
                    fontsize=11, fontweight="bold")
        ax.annotate(f"{b:.1f}", (i + w / 2, b + 0.8), ha="center", color=INK,
                    fontsize=11, fontweight="bold")
        ax.annotate(f"{a / b:.1f}x", (i, max(a, b) + 4), ha="center", color=MUTED,
                    fontsize=11)
    ax.set_xticks(x)
    ax.set_xticklabels([str(t) for t in THREADS])
    ax.set_ylim(0, 45)
    ax.set_xlabel("Threads", color=MUTED, fontsize=11)
    ax.set_ylabel("Million embeddings per second", color=MUTED, fontsize=11)
    ax.set_title("Same query, same sink, same 139M embeddings delivered",
                 color=INK, fontsize=14, fontweight="bold", loc="left", pad=14)
    ax.legend(frameon=False, loc="upper left", fontsize=11, labelcolor=INK)
    save(fig, outdir, "gsplit_vs_qsplit")


csv_path, outdir = sys.argv[1], sys.argv[2]
table = load(csv_path)
eps_vs_threads(table, outdir)
consolidation(table, outdir)
gsplit_vs_qsplit(table, outdir)

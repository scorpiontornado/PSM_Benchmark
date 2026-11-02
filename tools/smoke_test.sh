#!/usr/bin/env bash
# Run every combination once on a screened HPRD query, then print PASSED or FAILED.
#
# Usage: bash tools/smoke_test.sh <datasets_dir> <results_dir> [max_threads]
# Run from the repo root. max_threads (default 64) drops the runs above it,
# for machines where 64 threads aren't allowed.
set -euo pipefail

datasets=$1
results=$2
max_threads=${3:-64}
hprd=$datasets/HPRD/L15

python3 tools/screen_queries.py build/matching/SubgraphMatching.out \
    "$hprd/HPRD-15.txt" 3 "$hprd"/Q10/bin{1,2,3}

mkdir -p "$results"
python3 tools/make_plan.py smoke "$datasets" "$results/plan.csv" "$max_threads"
python3 tools/run_plan.py "$results/plan.csv" "$results" | tee "$results/run.log"

if grep -qE "FAILED|COUNTS DISAGREE|STORED != COUNTED" "$results/run.log"; then
    echo "FAILED: see the lines above, or $results/run.log"
    exit 1
fi
echo PASSED

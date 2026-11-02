#!/usr/bin/env bash
# Download the datasets, convert them to driver graph files and generate query graphs.
#
# Usage: bash tools/fetch_datasets.sh <out_dir>
#
# Writes <out_dir>/<graph>/L<labels>/<graph>-<labels>.txt and Q<size>/bin<lo>/Q<size>-<i>.txt.
# Downloads are saved to <out_dir>/raw, and aren't re-downloaded on reruns.
set -euo pipefail

OUT=$1
SEED=0
TOOLS=$(cd "$(dirname "$0")" && pwd)
RAW=$OUT/raw
mkdir -p "$RAW"

# fetch <url>: download into $RAW unless already there.
fetch() {
    file=$RAW/$(basename "$1")
    [ -f "$file" ] || { curl -fL -o "$file.part" "$1" && mv "$file.part" "$file"; }
}

# queries <graph> <labels> <count> <size>...: <count> queries per density bin
# (average degree [1,2), [2,3), [3,inf)) into Q<size>/bin<lo>/.
queries() {
    local dir=$OUT/$1/L$2
    for size in "${@:4}"; do
        for bin in "1 2" "2 3" "3 inf"; do
            read -r lo hi <<< "$bin"
            python3 "$TOOLS/gen_queries.py" "$dir/$1-$2.txt" "$size" "$3" "$dir/Q$size/bin$lo" \
                --seed "$SEED" --min-degree "$lo" --max-degree "$hi"
        done
    done
}

# HPRD and Human with their native labels, from VEQ's copy of the Sun and Luo graphs.
fetch https://github.com/SNUCSE-CTA/VEQ/raw/master/subgraphMatchingData.zip
python3 -m zipfile -e "$RAW/subgraphMatchingData.zip" "$RAW/veq"
mkdir -p "$OUT/HPRD/Lnative" "$OUT/human/Lnative"
python3 "$TOOLS/native_to_graph.py" "$RAW/veq/hprd.gfu" "$OUT/HPRD/Lnative/HPRD-native.txt"
python3 "$TOOLS/native_to_graph.py" "$RAW/veq/human.gfu" "$OUT/human/Lnative/human-native.txt"
grep '^e ' "$OUT/HPRD/Lnative/HPRD-native.txt" | cut -d' ' -f2,3 > "$RAW/hprd.edges"
grep '^e ' "$OUT/human/Lnative/human-native.txt" | cut -d' ' -f2,3 > "$RAW/human.edges"

# The same graphs with uniformly random labels (HPRD is also generated at every label count).
for labels in 1 5 15 30 45 60; do
    mkdir -p "$OUT/HPRD/L$labels"
    python3 "$TOOLS/edgelist_to_graph.py" "$RAW/hprd.edges" "$labels" "$OUT/HPRD/L$labels/HPRD-$labels.txt" --seed "$SEED"
done
mkdir -p "$OUT/human/L15"
python3 "$TOOLS/edgelist_to_graph.py" "$RAW/human.edges" 15 "$OUT/human/L15/human-15.txt" --seed "$SEED"

# SNAP com-DBLP, com-Youtube, com-Orkut and twitch_gamers, random labels
# (DBLP is also generated at every label count).
for g in dblp youtube orkut; do
    fetch https://snap.stanford.edu/data/bigdata/communities/com-$g.ungraph.txt.gz
done
for labels in 1 5 15 30 45 60; do
    mkdir -p "$OUT/dblp/L$labels"
    python3 "$TOOLS/edgelist_to_graph.py" "$RAW/com-dblp.ungraph.txt.gz" "$labels" "$OUT/dblp/L$labels/dblp-$labels.txt" --seed "$SEED"
done
for g in youtube orkut; do
    mkdir -p "$OUT/$g/L15"
    python3 "$TOOLS/edgelist_to_graph.py" "$RAW/com-$g.ungraph.txt.gz" 15 "$OUT/$g/L15/$g-15.txt" --seed "$SEED"
done

fetch https://snap.stanford.edu/data/twitch_gamers.zip
python3 -m zipfile -e "$RAW/twitch_gamers.zip" "$RAW/twitch"
mkdir -p "$OUT/twitch/L15"
python3 "$TOOLS/edgelist_to_graph.py" "$RAW/twitch/large_twitch_edges.csv" 15 "$OUT/twitch/L15/twitch-15.txt" --seed "$SEED"

# LDBC LSQB SF10, vertex type as label. The CSVs and sort files (about 15 GB) go to
# $TMPDIR, default /dev/shm (RAM), because the experiment disk is nearly full.
# TODO: standardise once more space becomes available
name=social-network-sf10-projected-fk
TMP=${TMPDIR:-/dev/shm}
fetch "https://datasets.ldbcouncil.org/lsqb/$name.tar.zst"
zstd -dc "$RAW/$name.tar.zst" | tar -x -f - -C "$TMP"
mkdir -p "$OUT/ldbc-sf10/Lnative"
TMPDIR=$TMP python3 "$TOOLS/lsqb_to_graph.py" "$TMP/$name" "$OUT/ldbc-sf10/Lnative/ldbc-sf10-native.txt"
rm -r "${TMP:?}/$name"

# Main graphs: 50 per bin for screening. Label and size arms.
# Large graphs: only the first query per bin is used.
for g in "HPRD 15" "human 15" "dblp 15" "youtube 15" "HPRD native" "human native"; do
    queries $g 50 10
done
for g in HPRD dblp; do
    queries "$g" 15 3 20 30
    for labels in 1 5 30 45 60; do
        queries "$g" "$labels" 3 10
    done
done
for g in "twitch 15" "orkut 15" "ldbc-sf10 native"; do
    queries $g 3 10
done

# PSM_Benchmark

Source code of the parallel subgraph matching study of Yu et al., *"Characterizing
Parallel Subgraph Matching Performance"* (PVLDB 19(7), 2026). The main function is in
`matching/StudyPerformance.cpp`.

## Build

Needs Linux on x86-64, a C++20 compiler, CMake, OpenMP and pthreads. The build passes
`-march=native` and the set-intersection kernels use AVX2 intrinsics, so x86-64 is the
only supported target.

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j8
```

The benchmark driver lands at `build/matching/SubgraphMatching.out`. The dataset
conversion and preparation tools (`GraphConverter.out`, `EdgeListConverter.out`,
`ReassignLabel`, `SelectEdge`) land in the same directory.

## Run

```shell
./build/matching/SubgraphMatching.out \
  -d datasets/HPRD/L15/HPRD-15.txt -q datasets/HPRD/L15/Q10/Q10-1.txt \
  -QorCandi C -split linear -schedule busy2idlenostop \
  -BackMethods DPiso_DPiso_LFTJ -threadnums 1,2,4,8 -num MAX \
  -time_limit 5 -OutputFile out.txt
```

All output goes to `-OutputFile`, not stdout. One process sweeps `-threadnums` and
`-BackMethods` internally, loading the graphs once.

Main options, with the full list in `matching/matchingcommand.h`:

| Flag | Meaning |
|---|---|
| `-d`, `-q` | data graph and query graph files |
| `-QorCandi` | `C` splits the candidate space (GSplit), `Q` splits the query graph (QSplit) |
| `-split` | `linear`, `workload`, `upperone`, `layer` |
| `-schedule` | `static`, `staticworkload`, `busy2idlenostop`, `busy2idledepthstop`, `timeout` |
| `-BackMethods` | `<filter>_<order>_<engine>` triples, e.g. `DPiso_DPiso_LFTJ` |
| `-threadnums` | comma-separated thread counts to sweep |
| `-num` | embedding limit, or `MAX` |
| `-time_limit` | seconds |
| `-mode` | `count` counts embeddings, `match` (default) stores them |
| `-sink` | how a stored embedding is delivered: `threadlocal` (default), `globallock` |

Not every split x schedule pair is valid; `valid_combinations` in
`test_anony/survey_bash/C_exp.py` is the authoritative list. Some behaviour is set at
compile time in `configuration/config.h` rather than by flag, and changing it requires a
full rebuild.

Graph files are plain text: `t <vertex_count> <edge_count>`, then one
`v <id> <label> <degree>` line per vertex, then one `e <src> <dst>` line per edge.

### `-mode` and `-sink`

`-mode` selects the problem being solved. `count` increments a counter and stores
nothing, so its throughput excludes the cost of delivering results. `match` stores every
embedding it finds. Both split modes honour both settings, so their throughput is
comparable: same kernels, same splitting, same scheduling, differing only in how the
query is split. `-mode count` is rejected under `-QorCandi Q`, because the join phase
consumes the per-unit result tables and produces nothing without them.

`-sink` selects how a stored embedding is delivered. `globallock` uses one mutex and one
growing vector per query unit, which is what `UnitArgs::addPartialMatch` does;
`threadlocal` uses per-worker block buffers, copied into the unit tables after every
worker has been joined. That copy is timed separately and reported as
`Consolidation time (seconds)`, so a comparison of the two sinks includes it. Sink
statistics are written to the output file as a `Result mode: ...` line.

`tools/compare.py` runs one binary over the split-mode x delivery matrix and writes a
CSV, taking the binary path and a label so control and thesis results stay separate.

## Full sweep

```shell
cd test_anony/survey_bash && python3 C_exp.py    # needs tqdm, psutil
```

Results land in `test_anony/C_output/`, failures in `C_output/error.txt`, peak RSS in
`C_output/C_memory.txt`. The sweep lists are hardcoded near the top of `main()`.

## Tests

The GoogleTest suite in `matching/test/` is disabled: `add_subdirectory(test)` is
commented out in `matching/CMakeLists.txt` and `matching/test/testConfig.h` hardcodes
dataset paths from the original authors' machine.

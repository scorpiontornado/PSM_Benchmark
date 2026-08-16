#ifndef MATCHING_MATERIALIZER_H
#define MATCHING_MATERIALIZER_H

#include "configuration/types.h"

#include <cstddef>

class UnitArgs;

/*
    Result sink for the enumeration kernels.

    Two independent questions, one flag each:

      -mode count | match   which problem is being solved. count increments a
                            counter and stores nothing; match stores every
                            embedding it finds.
      -sink globallock | threadlocal
                            how a delivered embedding is stored. globallock is
                            the artifact's own mechanism, one mutex per unit
                            guarding one growing vector. threadlocal gives each
                            worker its own block buffers, copied into the unit's
                            table by consolidate() once the workers are joined.

    Both split modes go through here, so their throughput is comparable: the
    same kernels, the same sink, differing only in how the query is split.
    QSplit rejects -mode count, because the join phase consumes the per-unit
    tables and produces nothing without them.

    One emit site is deliberately not materialized: bsxGenResult (Circinus with
    frozen sets) adds a whole compressed group to the embedding count without
    ever forming the individual embeddings. Under that engine storedCount() is
    lower than the reported embedding count.
*/
enum ResultMode {
    RESULT_COUNT = 0, // count embeddings; store nothing
    RESULT_MATCH = 1, // store every embedding
};

enum SinkType {
    SINK_GLOBAL_LOCK = 0,  // one mutex + one growing vector per unit
    SINK_THREAD_LOCAL = 1, // per-worker block buffers, consolidated afterwards
};

inline int RESULT_MODE = ResultMode::RESULT_MATCH;
inline int SINK_TYPE = SinkType::SINK_THREAD_LOCAL;

class Materializer {
  public:
    // embeddings per thread-local block
    static constexpr size_t BLOCK_EMBEDDINGS = 4096;

    // Register the units whose tables receive the results. Call once per query,
    // after the unit args are built and before enumeration starts.
    static void init(UnitArgs** unit_args_vec, int unit_count);

    // Store one embedding of `length` ui values, indexed by query vertex id.
    // The mode test is inlined so that a count run does not pay a call in the
    // innermost enumeration loop.
    static void record(int id_unit, const ui* embedding, int length) {
        if (RESULT_MODE == ResultMode::RESULT_MATCH) {
            recordSlow(id_unit, embedding, length);
        }
    }

    // Move the thread-local blocks into each unit's PartialMatch, freeing each
    // block as it is copied. A no-op under SINK_GLOBAL_LOCK, which writes there
    // directly. Must run after every worker has been joined and before anything
    // reads the unit tables.
    static void consolidate();

    // Free everything stored and zero the statistics. Call once per query and
    // per thread count, before enumeration starts.
    static void reset();

    // Embeddings stored since the last reset(), whether they are still in
    // thread-local blocks or already in the unit tables.
    static size_t storedCount();

    // Bytes held by the sinks. Unit tables are counted by vector capacity, and
    // thread-local blocks whole, including the unused tail of each last block.
    static size_t storedBytes();

    // Log the materialization statistics: mode, sink, embeddings, bytes, worker
    // sinks, blocks.
    static void logStatistics();

  private:
    static void recordSlow(int id_unit, const ui* embedding, int length);
};

#endif // MATCHING_MATERIALIZER_H

#ifndef MATCHING_MATERIALIZER_H
#define MATCHING_MATERIALIZER_H

#include "configuration/types.h"

#include <cstddef>
#include <mutex>
#include <vector>

/*
    Under GSplit (C) the enumeration kernels only increment l_embedding_count;
    the embedding itself is never written anywhere, so EPS excludes the cost of
    delivering results. These three sinks make counting-EPS and materialized-EPS
    comparable on identical kernels.
*/
enum MaterializeMode {
    MAT_NONE = 0,         // count only
    MAT_GLOBAL_LOCK = 1,  // one global lock + one shared vector, as QSplit's addPartialMatch does
    MAT_THREAD_LOCAL = 2, // per-worker block buffers, consolidated after enumeration
};

inline int MATERIALIZE_MODE = MaterializeMode::MAT_NONE;

class Materializer {
  public:
    // embeddings per block
    static const size_t BLOCK_EMBEDDINGS = 4096;

    // Store one embedding of `length` ui values. Returns immediately under MAT_NONE.
    static void record(const ui* embedding, int length);

    // Free everything stored and zero the statistics. Call once per query and
    // per thread count, before enumeration starts.
    static void reset();

    static size_t storedCount();

    static size_t storedBytes();

    // Log the materialization statistics: embeddings, bytes, blocks, worker sinks.
    static void logStatistics();
};

#endif // MATCHING_MATERIALIZER_H

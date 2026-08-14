#ifndef MATCHING_MATERIALIZER_H
#define MATCHING_MATERIALIZER_H

#include "configuration/types.h"

#include <cstddef>
#include <mutex>
#include <vector>

/*
    Result sink for the enumeration kernels, selected by -materialize.

    Under GSplit (-QorCandi C) the kernels only do l_embedding_count += 1; the
    embedding itself is never written anywhere, so the reported EPS measures
    finding embeddings rather than delivering them. Selecting a mode other than
    MAT_NONE makes every kernel store each embedding it finds, so counting-EPS
    and materializing-EPS can be compared on identical kernels, splitting and
    scheduling.

    GSplit only. Under QSplit (-QorCandi Q) the kernels already store every
    embedding through UnitArgs::addPartialMatch, because the join phase consumes
    them; StudyPerformance forces MAT_NONE there rather than store each
    embedding twice.

    One emit site is deliberately not materialized: bsxGenResult (Circinus with
    frozen sets) adds a whole compressed group to the embedding count without
    ever forming the individual embeddings. Under that engine storedCount() is
    lower than the reported embedding count.
*/
enum MaterializeMode {
    MAT_NONE = 0,         // count only; no embedding is stored anywhere
    MAT_GLOBAL_LOCK = 1,  // one global lock + one shared vector, as QSplit's addPartialMatch does
    MAT_THREAD_LOCAL = 2, // per-worker block buffers, consolidated after enumeration
};

inline int MATERIALIZE_MODE = MaterializeMode::MAT_NONE;

class Materializer {
  public:
    // embeddings per thread-local block
    static constexpr size_t BLOCK_EMBEDDINGS = 4096;

    // Store one embedding of `length` ui values, indexed by query vertex id.
    // The mode test is inlined so that a MAT_NONE run does not pay a call in
    // the innermost enumeration loop.
    static void record(const ui* embedding, int length) {
        if (MATERIALIZE_MODE != MaterializeMode::MAT_NONE) {
            store(embedding, length);
        }
    }

    // Free everything stored and zero the statistics. Call once per query and
    // per thread count, before enumeration starts.
    static void reset();

    // Embeddings stored since the last reset().
    static size_t storedCount();

    // Bytes held by the sinks. Thread-local blocks are counted whole, including
    // the unused tail of each worker's last block.
    static size_t storedBytes();

    // Log the materialization statistics: mode, embeddings, bytes, worker
    // sinks, blocks.
    static void logStatistics();

  private:
    static void store(const ui* embedding, int length);
};

#endif // MATCHING_MATERIALIZER_H

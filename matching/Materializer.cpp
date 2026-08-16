#include "Materializer.h"
#include "Args.h"
#include "utility/relations/partialMatch.h"
#include "utility/statistics/Logs.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

// The units whose PartialMatch tables receive the results, set by init().
UnitArgs** g_unit_args_vec = nullptr;
int g_unit_count = 0;

// SINK_THREAD_LOCAL: one sink per (worker, unit). The blocks are owned by the
// registry, not by the thread, so the totals survive the workers being joined.
struct ThreadSink {
    std::vector<ui*> blocks;
    size_t count{0};         // embeddings stored
    size_t used_in_block{0}; // embeddings used in the current block
    int id_unit{0};
    int length{0}; // ui values per embedding
};

std::mutex g_registry_mutex;
std::vector<ThreadSink*> g_registry;
// reset() frees every sink; the generation stops a still-running thread from
// reusing the dangling pointers its thread_local slot still holds.
std::atomic<uint64_t> g_generation{0};
thread_local std::vector<ThreadSink*> t_sinks;
thread_local uint64_t t_generation = 0;

// Kept across consolidate(), which frees the sinks themselves.
std::atomic<size_t> g_sink_count{0};
std::atomic<size_t> g_block_count{0};

ThreadSink* acquireSink(int id_unit, int length) {
    uint64_t generation = g_generation.load(std::memory_order_acquire);
    if (t_generation != generation) {
        t_sinks.clear();
        t_generation = generation;
    }
    if ((size_t)id_unit >= t_sinks.size()) {
        t_sinks.resize(id_unit + 1, nullptr);
    }
    if (t_sinks[id_unit] == nullptr) {
        ThreadSink* sink = new ThreadSink();
        sink->id_unit = id_unit;
        sink->length = length;
        t_sinks[id_unit] = sink;
        std::unique_lock<std::mutex> lock(g_registry_mutex);
        g_registry.push_back(sink);
        g_sink_count.fetch_add(1, std::memory_order_relaxed);
    }
    return t_sinks[id_unit];
}

} // namespace

void
Materializer::init(UnitArgs** unit_args_vec, int unit_count) {
    g_unit_args_vec = unit_args_vec;
    g_unit_count = unit_count;
}

void
Materializer::recordSlow(int id_unit, const ui* embedding, int length) {
    if (SINK_TYPE == SinkType::SINK_GLOBAL_LOCK) {
        // The artifact's own mechanism, unchanged: one mutex per unit guarding
        // one vector that reallocates wholesale.
        g_unit_args_vec[id_unit]->addPartialMatch(const_cast<ui*>(embedding));
        return;
    }

    ThreadSink* sink = acquireSink(id_unit, length);
    if (sink->blocks.empty() || sink->used_in_block == BLOCK_EMBEDDINGS) {
        sink->blocks.push_back(new ui[BLOCK_EMBEDDINGS * (size_t)length]);
        sink->used_in_block = 0;
        g_block_count.fetch_add(1, std::memory_order_relaxed);
    }
    ui* slot = sink->blocks.back() + sink->used_in_block * (size_t)length;
    std::memcpy(slot, embedding, (size_t)length * sizeof(ui));
    sink->used_in_block += 1;
    sink->count += 1;
}

void
Materializer::consolidate() {
    if (SINK_TYPE != SinkType::SINK_THREAD_LOCAL) {
        return;
    }

    std::unique_lock<std::mutex> lock(g_registry_mutex);
    for (int id_unit = 0; id_unit < g_unit_count; id_unit++) {
        size_t total = 0;
        for (ThreadSink* sink : g_registry) {
            if (sink->id_unit == id_unit) {
                total += sink->count;
            }
        }
        if (total == 0) {
            continue;
        }

        PartialMatch* pm = g_unit_args_vec[id_unit]->unit_partial_match;
        // One allocation for the whole table; each block is freed as soon as it
        // has been copied, so the two representations do not both stay live.
        pm->data.reserve(pm->data.size() + total * pm->length);
        for (ThreadSink* sink : g_registry) {
            if (sink->id_unit != id_unit) {
                continue;
            }
            for (size_t i = 0; i < sink->blocks.size(); i++) {
                size_t rows = (i + 1 == sink->blocks.size()) ? sink->used_in_block : BLOCK_EMBEDDINGS;
                ui* block = sink->blocks[i];
                pm->data.insert(pm->data.end(), block, block + rows * (size_t)sink->length);
                delete[] block;
            }
            sink->blocks.clear();
            pm->size += sink->count;
        }
    }

    for (ThreadSink* sink : g_registry) {
        delete sink;
    }
    g_registry.clear();
    g_generation.fetch_add(1, std::memory_order_release);
}

void
Materializer::reset() {
    std::unique_lock<std::mutex> lock(g_registry_mutex);
    for (ThreadSink* sink : g_registry) {
        for (ui* block : sink->blocks) {
            delete[] block;
        }
        delete sink;
    }
    g_registry.clear();
    g_generation.fetch_add(1, std::memory_order_release);
    g_sink_count.store(0, std::memory_order_relaxed);
    g_block_count.store(0, std::memory_order_relaxed);
}

size_t
Materializer::storedCount() {
    size_t total = 0;
    for (int id_unit = 0; id_unit < g_unit_count; id_unit++) {
        total += g_unit_args_vec[id_unit]->unit_partial_match->size;
    }

    std::unique_lock<std::mutex> lock(g_registry_mutex);
    for (ThreadSink* sink : g_registry) {
        total += sink->count;
    }
    return total;
}

size_t
Materializer::storedBytes() {
    size_t total = 0;
    for (int id_unit = 0; id_unit < g_unit_count; id_unit++) {
        total += g_unit_args_vec[id_unit]->unit_partial_match->data.capacity() * sizeof(ui);
    }

    std::unique_lock<std::mutex> lock(g_registry_mutex);
    for (ThreadSink* sink : g_registry) {
        total += sink->blocks.size() * BLOCK_EMBEDDINGS * (size_t)sink->length * sizeof(ui);
    }
    return total;
}

void
Materializer::logStatistics() {
    const char* mode_name = RESULT_MODE == ResultMode::RESULT_MATCH ? "match" : "count";
    const char* sink_name = SINK_TYPE == SinkType::SINK_THREAD_LOCAL ? "threadlocal" : "globallock";

    LOG() << "Result mode: " << mode_name
          << ", sink: " << sink_name
          << ", stored embeddings: " << storedCount()
          << ", stored bytes: " << storedBytes()
          << ", worker sinks: " << g_sink_count.load(std::memory_order_relaxed)
          << ", blocks: " << g_block_count.load(std::memory_order_relaxed) << std::endl;
}

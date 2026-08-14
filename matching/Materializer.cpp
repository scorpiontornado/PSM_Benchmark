#include "Materializer.h"
#include "utility/statistics/Logs.h"

#include <atomic>
#include <cstdint>
#include <cstring>

namespace {

// MAT_GLOBAL_LOCK: one lock guarding one vector that reallocates wholesale,
// matching what UnitArgs::addPartialMatch does.
std::mutex g_store_mutex;
std::vector<ui> g_store;
size_t g_store_count = 0;

// MAT_THREAD_LOCAL: one sink per worker. The blocks are owned by the registry,
// not by the thread, so the totals survive the workers being joined.
struct ThreadSink {
    std::vector<ui*> blocks;
    size_t count{0};              // embeddings stored
    size_t used_in_block{0};      // embeddings used in the current block
    int length{0};                // ui values per embedding
    uint64_t generation{0};
};

std::mutex g_registry_mutex;
std::vector<ThreadSink*> g_registry;
// reset() frees every sink; the generation stops a still-running thread from
// reusing the dangling pointer its thread_local slot still holds.
std::atomic<uint64_t> g_generation{0};
thread_local ThreadSink* t_sink = nullptr;

ThreadSink* acquireSink(int length) {
    uint64_t generation = g_generation.load(std::memory_order_acquire);
    if (t_sink == nullptr || t_sink->generation != generation) {
        t_sink = new ThreadSink();
        t_sink->length = length;
        t_sink->generation = generation;
        std::unique_lock<std::mutex> lock(g_registry_mutex);
        g_registry.push_back(t_sink);
    }
    return t_sink;
}

} // namespace

void
Materializer::store(const ui* embedding, int length) {
    if (MATERIALIZE_MODE == MaterializeMode::MAT_GLOBAL_LOCK) {
        std::unique_lock<std::mutex> lock(g_store_mutex);
        g_store.insert(g_store.end(), embedding, embedding + length);
        g_store_count += 1;
        return;
    }

    ThreadSink* sink = acquireSink(length);
    if (sink->blocks.empty() || sink->used_in_block == BLOCK_EMBEDDINGS) {
        sink->blocks.push_back(new ui[BLOCK_EMBEDDINGS * (size_t)length]);
        sink->used_in_block = 0;
    }
    ui* slot = sink->blocks.back() + sink->used_in_block * (size_t)length;
    std::memcpy(slot, embedding, (size_t)length * sizeof(ui));
    sink->used_in_block += 1;
    sink->count += 1;
}

void
Materializer::reset() {
    {
        std::unique_lock<std::mutex> lock(g_store_mutex);
        std::vector<ui>().swap(g_store);
        g_store_count = 0;
    }

    std::unique_lock<std::mutex> lock(g_registry_mutex);
    for (ThreadSink* sink : g_registry) {
        for (ui* block : sink->blocks) {
            delete[] block;
        }
        delete sink;
    }
    g_registry.clear();
    g_generation.fetch_add(1, std::memory_order_release);
    t_sink = nullptr;
}

size_t
Materializer::storedCount() {
    if (MATERIALIZE_MODE == MaterializeMode::MAT_GLOBAL_LOCK) {
        std::unique_lock<std::mutex> lock(g_store_mutex);
        return g_store_count;
    }

    std::unique_lock<std::mutex> lock(g_registry_mutex);
    size_t total = 0;
    for (ThreadSink* sink : g_registry) {
        total += sink->count;
    }
    return total;
}

size_t
Materializer::storedBytes() {
    if (MATERIALIZE_MODE == MaterializeMode::MAT_GLOBAL_LOCK) {
        std::unique_lock<std::mutex> lock(g_store_mutex);
        return g_store.capacity() * sizeof(ui);
    }

    std::unique_lock<std::mutex> lock(g_registry_mutex);
    size_t total = 0;
    for (ThreadSink* sink : g_registry) {
        total += sink->blocks.size() * BLOCK_EMBEDDINGS * (size_t)sink->length * sizeof(ui);
    }
    return total;
}

void
Materializer::logStatistics() {
    size_t sink_count = 0;
    size_t block_count = 0;
    {
        std::unique_lock<std::mutex> lock(g_registry_mutex);
        sink_count = g_registry.size();
        for (ThreadSink* sink : g_registry) {
            block_count += sink->blocks.size();
        }
    }

    const char* mode_name = MATERIALIZE_MODE == MaterializeMode::MAT_GLOBAL_LOCK   ? "globallock"
                            : MATERIALIZE_MODE == MaterializeMode::MAT_THREAD_LOCAL ? "threadlocal"
                                                                                    : "none";

    LOG() << "Materialize mode: " << mode_name
          << ", stored embeddings: " << storedCount()
          << ", stored bytes: " << storedBytes()
          << ", worker sinks: " << sink_count
          << ", blocks: " << block_count << std::endl;
}

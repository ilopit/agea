// Microbenchmarks for the lock-free chunked slot_storage + handle_allocator.
//
// Build RELEASE for meaningful numbers: in Debug the thread_affinity guard and
// KRG_check_debug are live and the methods aren't inlined, so a Debug run measures
// the harness, not the structure. Under NDEBUG both compile out, so this reports
// the real cost: at() is one acquire load + address math; valid() is two acquire
// loads + a compare.
//
// Two access patterns:
//   *_sequential  walks slots in order -> prefetcher hides the latency (best case).
//   *_random      walks a shuffled index list -> defeats the prefetcher, so the
//                 number reflects real cache behavior for a working set that does
//                 NOT fit in L1/L2 (a few MB). This is the figure that matters for
//                 a draw loop dereferencing scattered handles.
//
//   tools/build.sh -r render_types_bench && tools/run.sh -r render_types_bench.exe

#include "render_types/handle_pool.h"
#include "render_types/render_handle.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

using namespace kryga::render::types;

namespace
{
struct fake_mesh
{
    int verts = 0;
    int indices = 0;
};

constexpr uint8_t KIND_MESH = 0;
using mesh_handle = handle<KIND_MESH>;
using pool_storage = slot_storage<KIND_MESH, fake_mesh>;
using mesh_alloc = handle_allocator<KIND_MESH>;

// 2^19 slots * sizeof(slot) (fake_mesh 8 B + gen 4 B = 12 B) ~= 6 MB. Comfortably
// past L2 (1 MB) so the random walk actually misses; power of two for cheap wrap.
constexpr uint32_t kWorkingSet = 1u << 19;
constexpr uint32_t kGrowCount = 4096;  // smaller: the grow bench rebuilds per iter

// A storage pre-populated with kWorkingSet live slots, the handles to them, and a
// shuffled index list for the random-access benchmarks.
struct populated
{
    pool_storage storage;
    mesh_alloc alloc{storage};
    std::vector<mesh_handle> handles;
    std::vector<uint32_t> shuffled;  // slot indices in random order
    uint32_t chain_start = 0;        // entry into the pointer-chase cycle

    populated()
    {
        handles.reserve(kWorkingSet);
        shuffled.reserve(kWorkingSet);
        for (uint32_t i = 0; i < kWorkingSet; ++i)
        {
            auto h = alloc.reserve();
            storage.set_generation(h.index(), h.generation());
            handles.push_back(h);
            shuffled.push_back(h.index());
        }
        std::mt19937 rng(0xC0FFEE);  // fixed seed -> reproducible miss pattern
        std::shuffle(shuffled.begin(), shuffled.end(), rng);

        // Link the shuffled slots into one random cycle: each slot's `verts` holds
        // the index of the NEXT slot to visit. Chasing this serializes the loads
        // (next address depends on the current load), so a chase measures per-access
        // LATENCY -- unlike the independent-load benches, which measure throughput.
        for (uint32_t k = 0; k < kWorkingSet; ++k)
        {
            uint32_t cur = shuffled[k];
            uint32_t next = shuffled[(k + 1) & (kWorkingSet - 1)];
            storage.at(cur)->verts = static_cast<int>(next);
        }
        chain_start = shuffled[0];
    }
};
}  // namespace

// Pure deref, sequential: prefetcher-friendly best case.
static void
BM_storage_at_sequential(benchmark::State& state)
{
    populated p;
    uint32_t i = 0;
    for (auto _ : state)
    {
        uint32_t idx = p.handles[i & (kWorkingSet - 1)].index();
        benchmark::DoNotOptimize(p.storage.at(idx)->verts);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_storage_at_sequential);

// Pure deref, random: walks the shuffled index list -> cache misses dominate.
static void
BM_storage_at_random(benchmark::State& state)
{
    populated p;
    uint32_t i = 0;
    for (auto _ : state)
    {
        uint32_t idx = p.shuffled[i & (kWorkingSet - 1)];
        benchmark::DoNotOptimize(p.storage.at(idx)->verts);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_storage_at_random);

// Pure deref, pointer-chase: each read's index comes from the previous read, so
// loads can't overlap -> this measures per-access LATENCY over the ~6 MB set, the
// real cost of one cache miss. Contrast with at_random (independent -> throughput).
static void
BM_storage_at_pointer_chase(benchmark::State& state)
{
    populated p;
    uint32_t idx = p.chain_start;
    for (auto _ : state)
    {
        idx = static_cast<uint32_t>(p.storage.at(idx)->verts);
        benchmark::DoNotOptimize(idx);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_storage_at_pointer_chase);

// Liveness check, random: size load + chunk load + generation compare, scattered.
static void
BM_storage_valid_random(benchmark::State& state)
{
    populated p;
    uint32_t i = 0;
    for (auto _ : state)
    {
        uint32_t idx = p.shuffled[i & (kWorkingSet - 1)];
        benchmark::DoNotOptimize(p.storage.valid(p.handles[idx]));
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_storage_valid_random);

// Steady-state allocator hot path: reserve + immediate reclaim reuses one slot off
// the free-list (no growth), measuring the free-list pop/push + generation stamp.
static void
BM_allocator_reserve_reclaim(benchmark::State& state)
{
    pool_storage storage;
    mesh_alloc alloc{storage};
    alloc.reclaim(alloc.reserve());  // warm one slot onto the free-list

    for (auto _ : state)
    {
        auto h = alloc.reserve();
        benchmark::DoNotOptimize(h.v);
        alloc.reclaim(h);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_allocator_reserve_reclaim);

// Reserve WITH growth: a fresh allocator reserving up to N forces a chunk append
// every chunk_size slots. Amortized cost of the grow path (+ per-iter teardown).
static void
BM_allocator_reserve_growing(benchmark::State& state)
{
    for (auto _ : state)
    {
        pool_storage storage;
        mesh_alloc alloc{storage};
        for (uint32_t i = 0; i < kGrowCount; ++i)
        {
            benchmark::DoNotOptimize(alloc.reserve().v);
        }
    }
    state.SetItemsProcessed(state.iterations() * kGrowCount);
}
BENCHMARK(BM_allocator_reserve_growing);

#include "render_types/handle_pool.h"
#include "render_types/render_handle.h"

#include <atomic>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

using namespace kryga::render::types;

// Vocabulary: aliases are 8 bytes, trivially copyable, and distinct types so
// passing a mesh handle where a texture handle is expected won't compile.
static_assert(sizeof(mesh_handle) == 8, "handle must be a U64");
static_assert(std::is_trivially_copyable_v<mesh_handle>, "handle must be trivially copyable");
static_assert(!std::is_same_v<mesh_handle, texture_handle>, "kinds must be distinct types");
static_assert(!std::is_same_v<mesh_handle, material_handle>, "kinds must be distinct types");

namespace
{
struct fake_mesh
{
    int verts = 0;
    int indices = 0;
};

constexpr uint8_t KIND_MESH = 0;
constexpr uint8_t KIND_TEX = 1;

using mesh_handle = handle<KIND_MESH>;
using pool_storage = slot_storage<KIND_MESH, fake_mesh>;
using mesh_alloc = handle_allocator<KIND_MESH>;

// The split is now two objects: render-thread storage + model-thread allocator
// bound to it. This pair models a single content pool.
struct mesh_pool
{
    // Reserve + populate the slot's data, mirroring the generation shadow as the
    // render thread does at populate time. Keeps the tests reading like the old
    // single-object pool while exercising both halves.

    mesh_handle
    reserve_and_populate()
    {
        auto h = alloc.reserve();
        storage.set_generation(h.index(), h.generation());
        return h;
    }

    pool_storage storage;
    mesh_alloc alloc{storage};
};
}  // namespace

TEST(handle_pool, packing_roundtrip)
{
    auto h = mesh_handle::make(123, 456);
    EXPECT_EQ(h.index(), 123u);
    EXPECT_EQ(h.generation(), 456u);
    EXPECT_EQ(h.kind(), KIND_MESH);
    EXPECT_TRUE(static_cast<bool>(h));

    // Max-width fields round-trip without bleeding into neighbours.
    auto wide = handle<KIND_TEX>::make(mesh_handle::index_mask, 0xFFFFFFFFu);
    EXPECT_EQ(wide.index(), mesh_handle::index_mask);
    EXPECT_EQ(wide.generation(), 0xFFFFFFFFu);
    EXPECT_EQ(wide.kind(), KIND_TEX);
}

TEST(handle_pool, null_handle)
{
    mesh_handle h;
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.v, 0u);

    mesh_pool pool;
    EXPECT_FALSE(pool.alloc.valid(h));
    EXPECT_FALSE(pool.storage.valid(h));
}

// The defining new behavior: the allocator, owning no array of its own, grows the
// storage it was constructed from when it hands out a fresh index.
TEST(handle_allocator, reserve_grows_storage)
{
    mesh_pool pool;
    EXPECT_EQ(pool.storage.capacity(), 0u);

    auto h = pool.alloc.reserve();
    EXPECT_EQ(h.index(), 0u);
    EXPECT_GE(pool.storage.capacity(), 1u);  // allocator grew the storage to fit

    auto h2 = pool.alloc.reserve();
    EXPECT_EQ(h2.index(), 1u);
    EXPECT_GE(pool.storage.capacity(), 2u);
}

// Per-pool sizing: a small chunk_size forces growth across several chunk
// boundaries; every slot must still map to a distinct, addressable element. This
// is the "few meshes vs many materials" knob -- size each pool to its population.
TEST(slot_storage, custom_chunk_config)
{
    pool_storage st(/*chunk_size*/ 4, /*max_slots*/ 64);
    mesh_alloc a{st};

    std::vector<mesh_handle> hs;
    for (int i = 0; i < 10; ++i)  // 10 slots over 4-slot chunks -> 3 chunks
    {
        auto h = a.reserve();
        st.set_generation(h.index(), h.generation());
        st.at(h.index())->verts = static_cast<int>(h.index()) + 1000;
        hs.push_back(h);
    }

    EXPECT_GE(st.capacity(), 10u);
    for (auto h : hs)
    {
        EXPECT_TRUE(st.valid(h));
        EXPECT_EQ(st.at(h.index())->verts, static_cast<int>(h.index()) + 1000);
    }
}

TEST(handle_allocator, preallocate_sizes_both_halves)
{
    mesh_pool pool;
    pool.alloc.preallocate(16);

    EXPECT_EQ(pool.alloc.capacity(), 16u);    // allocator bookkeeping
    EXPECT_EQ(pool.storage.capacity(), 16u);  // storage array, sized in lockstep

    // After preallocate, reserve hands out ascending indices without growing.
    auto h = pool.alloc.reserve();
    EXPECT_EQ(h.index(), 0u);
    EXPECT_EQ(pool.storage.capacity(), 16u);  // no runtime growth
}

TEST(handle_pool, reserve_and_get)
{
    mesh_pool pool;
    auto h = pool.alloc.reserve();

    EXPECT_TRUE(static_cast<bool>(h));
    EXPECT_TRUE(pool.alloc.valid(h));
    EXPECT_EQ(pool.alloc.state(h), slot_state::reserved);
    EXPECT_EQ(pool.alloc.active_count(), 1u);

    // Reserved-but-not-populated: the allocator says valid (slot identity exists),
    // the storage says invalid (shadow generation still 0 -- nothing populated).
    EXPECT_FALSE(pool.storage.valid(h));

    // Populate (render side): mirror the generation, then write the data.
    pool.storage.set_generation(h.index(), h.generation());
    pool.storage.at(h.index())->verts = 99;

    EXPECT_TRUE(pool.storage.valid(h));
    EXPECT_EQ(pool.storage.at(h.index())->verts, 99);
}

TEST(handle_pool, reclaim_invalidates)
{
    mesh_pool pool;
    auto h = pool.reserve_and_populate();

    pool.alloc.reclaim(h);
    EXPECT_FALSE(pool.alloc.valid(h));  // generation bumped + slot free
    EXPECT_EQ(pool.alloc.active_count(), 0u);

    // Storage stays stale until the slot is reset/reused -- but the render side
    // can invalidate its shadow explicitly.
    pool.storage.reset(h.index());
    EXPECT_FALSE(pool.storage.valid(h));
}

TEST(handle_pool, slot_reuse_new_generation)
{
    mesh_pool pool;
    auto h1 = pool.alloc.reserve();
    auto idx1 = h1.index();
    pool.alloc.reclaim(h1);

    auto h2 = pool.alloc.reserve();
    EXPECT_EQ(h2.index(), idx1);                  // same slot recycled
    EXPECT_NE(h2.generation(), h1.generation());  // but new generation
    EXPECT_NE(h1.v, h2.v);

    EXPECT_FALSE(pool.alloc.valid(h1));  // stale handle to recycled slot -- rejected
    EXPECT_TRUE(pool.alloc.valid(h2));
    EXPECT_EQ(pool.alloc.capacity(), 1u);    // reuse, not growth
    EXPECT_EQ(pool.storage.capacity(), 1u);  // storage not grown either
}

TEST(handle_pool, reserve_many_distinct)
{
    mesh_pool pool;
    std::vector<mesh_handle> hs;
    for (int i = 0; i < 8; ++i)
    {
        hs.push_back(pool.alloc.reserve());
    }

    EXPECT_EQ(pool.alloc.active_count(), 8u);
    EXPECT_GE(pool.storage.capacity(), 8u);

    for (size_t i = 0; i < hs.size(); ++i)
    {
        EXPECT_TRUE(pool.alloc.valid(hs[i]));
        for (size_t j = i + 1; j < hs.size(); ++j)
        {
            EXPECT_NE(hs[i].v, hs[j].v);
        }
    }
}

TEST(handle_pool, state_transitions)
{
    mesh_pool pool;
    auto h = pool.alloc.reserve();

    pool.alloc.set_state(h, slot_state::pending);
    EXPECT_EQ(pool.alloc.state(h), slot_state::pending);

    pool.alloc.set_state(h, slot_state::resident);
    EXPECT_EQ(pool.alloc.state(h), slot_state::resident);
}

TEST(handle_pool, data_isolated_per_slot)
{
    mesh_pool pool;
    auto a = pool.reserve_and_populate();
    auto b = pool.reserve_and_populate();

    pool.storage.at(a.index())->verts = 10;
    pool.storage.at(b.index())->verts = 20;

    EXPECT_EQ(pool.storage.at(a.index())->verts, 10);
    EXPECT_EQ(pool.storage.at(b.index())->verts, 20);
}

// ---------------------------------------------------------------------------
// Concurrency: the model thread calls allocator.reserve() (which grows the
// storage by appending chunks + bumping the atomic size) while the render thread
// reads the storage lock-free. The chunked layout guarantees existing chunks are
// never moved, so a live slot stays readable across any number of concurrent
// growths. This path is NOT exercised by the single-threaded visual-regression
// suite, so it is validated here directly (run under TSan to prove the ordering).
// ---------------------------------------------------------------------------

// Model thread grows the storage continuously; render thread hammers a stable,
// populated slot. The stable slot must stay valid and readable throughout --
// i.e. concurrent growth never corrupts live reads.
TEST(handle_allocator, concurrent_grow_vs_read)
{
    mesh_pool pool;

    auto stable = pool.reserve_and_populate();
    pool.storage.at(stable.index())->verts = 4242;

    std::atomic<bool> stop{false};

    // Model thread: owns the allocator + drives storage growth (each fresh index
    // appends a chunk). Re-stamp both affinities to this thread first -- setup ran
    // on the main thread, so the guards are currently bound there. Bounded so the
    // chunk table doesn't balloon if the reader is slow.
    std::thread model(
        [&pool, &stop]
        {
            pool.alloc.bind_to_current_thread();
            pool.storage.bind_grower_to_current_thread();
            for (int i = 0; i < 200000 && !stop.load(std::memory_order_relaxed); ++i)
            {
                pool.alloc.reserve();
            }
        });

    // Render thread: the sole reader. Re-stamp the reader affinity to this thread.
    std::thread render(
        [&pool, stable, &stop]
        {
            pool.storage.bind_reader_to_current_thread();
            for (int i = 0; i < 200000; ++i)
            {
                ASSERT_TRUE(pool.storage.valid(stable));
                ASSERT_EQ(pool.storage.at(stable.index())->verts, 4242);
            }
            stop.store(true, std::memory_order_relaxed);
        });

    render.join();
    model.join();

    // Back on the main thread for the final reads -> re-stamp the reader affinity.
    pool.storage.bind_reader_to_current_thread();
    EXPECT_TRUE(pool.storage.valid(stable));
    EXPECT_EQ(pool.storage.at(stable.index())->verts, 4242);
}

// Many model threads reserving concurrently is NOT a supported pattern (the
// allocator's free-list is single-thread/model-owned). The supported cross-thread
// pattern is exactly one grower (model) vs readers (render), covered above.

// ---------------------------------------------------------------------------
// handle_allocator deferred free (the model-owned content allocator).
// free() invalidates immediately but parks the slot; tick() matures it after the
// deferral window so a slot is never reused while the GPU may still reference it.
// ---------------------------------------------------------------------------

TEST(handle_allocator, deferred_free_parks_then_matures)
{
    pool_storage st;
    mesh_alloc a{st};
    auto h0 = a.reserve();
    EXPECT_EQ(h0.index(), 0u);

    a.free(h0);
    EXPECT_FALSE(a.valid(h0));  // invalid the instant it's freed
    EXPECT_EQ(a.retiring_count(), 1u);

    // While parked the index must NOT be reused -- reserve grows instead.
    auto h1 = a.reserve();
    EXPECT_EQ(h1.index(), 1u);

    // defer_ticks == 3: two ticks isn't enough, the third matures slot 0.
    a.tick();
    a.tick();
    EXPECT_EQ(a.retiring_count(), 1u);
    a.tick();
    EXPECT_EQ(a.retiring_count(), 0u);

    // Slot 0 is now reusable, with a bumped generation; the old handle stays stale.
    auto h2 = a.reserve();
    EXPECT_EQ(h2.index(), 0u);
    EXPECT_NE(h2.generation(), h0.generation());
    EXPECT_TRUE(a.valid(h2));
    EXPECT_FALSE(a.valid(h0));
}

TEST(handle_allocator, immediate_reclaim_reuses_next_reserve)
{
    pool_storage st;
    mesh_alloc a{st};
    auto h0 = a.reserve();
    a.reclaim(h0);  // immediate path (system pool) -- no deferral
    EXPECT_FALSE(a.valid(h0));

    auto h1 = a.reserve();
    EXPECT_EQ(h1.index(), h0.index());  // reused right away
    EXPECT_NE(h1.generation(), h0.generation());
    EXPECT_EQ(a.retiring_count(), 0u);
}

TEST(handle_allocator, deferred_frees_mature_in_tick_order)
{
    pool_storage st;
    mesh_alloc a{st};
    auto h0 = a.reserve();  // idx 0
    auto h1 = a.reserve();  // idx 1
    auto h2 = a.reserve();  // idx 2

    a.free(h0);  // retired at tick 0
    a.tick();    // tick 1
    a.free(h1);  // retired at tick 1
    a.tick();    // tick 2
    a.tick();    // tick 3 -> h0 (0+3<=3) matures, h1 (1+3<=3? no) not yet
    EXPECT_EQ(a.retiring_count(), 1u);

    auto r0 = a.reserve();  // reuses slot 0
    EXPECT_EQ(r0.index(), 0u);

    a.tick();  // tick 4 -> h1 (1+3<=4) matures
    EXPECT_EQ(a.retiring_count(), 0u);
    auto r1 = a.reserve();  // reuses slot 1
    EXPECT_EQ(r1.index(), 1u);

    (void)h2;
}

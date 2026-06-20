#include "utils/handle.h"
#include "utils/laned_pool.h"
#include "render_types/render_handle.h"

#include <atomic>
#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

using namespace kryga::render::types;  // resource_kind + typed handle aliases
using namespace kryga::utils;           // handle / laned_storage / slot_state

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
}  // namespace

// The split-pool functional behaviour (reserve / reclaim / deferred-free / tick /
// claim epochs / forged-handle trust boundary) is exercised in test_split_pool_demo.
// This file pins the two things that live ONLY here: the raw handle bit-packing and
// the lock-free chunked growth machinery (detail::lane_store) under real threads.

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

    laned_storage<KIND_MESH, fake_mesh> storage{1};
    EXPECT_FALSE(storage.valid(h));  // gen-0 null never validates
}

// ---------------------------------------------------------------------------
// Concurrency: one thread grows a lane (appends chunks + bumps the atomic size)
// while another reads a stable, populated slot lock-free. The chunked layout
// guarantees existing chunks are never moved, so a live slot stays readable
// across any number of concurrent growths. This is the lock-free contract of the
// chunked storage that laned_storage builds on (detail::lane_store); it is NOT
// exercised by the single-threaded suites, so it is validated here directly (run
// under TSan to prove the publish ordering).
//
// In the steady-state split design grow + read run on the SAME consumer thread, so
// this races the two roles harder than production ever does -- a deliberate stress
// of the primitive's grower-vs-reader ordering.
// ---------------------------------------------------------------------------

TEST(lane_store, concurrent_grow_vs_read)
{
    laned_storage<KIND_MESH, fake_mesh> storage{1};
    auto& lane = storage.lane(0);

    // Populate a stable slot at local index 0, generation 1. These setup touches
    // bind the grower + reader affinity to THIS (main) thread; the worker threads
    // re-stamp their respective guard before their first touch.
    lane.grow_to(1);
    lane.set_generation(0, 1);
    lane.at(0)->verts = 4242;

    std::atomic<bool> stop{false};

    // Grower thread: append chunks continuously. Bounded so the chunk table doesn't
    // balloon if the reader is slow.
    std::thread grower(
        [&lane, &stop]
        {
            lane.bind_grower_to_current_thread();
            for (uint32_t i = 1; i < 200000 && !stop.load(std::memory_order_relaxed); ++i)
            {
                lane.grow_to(i + 1);
            }
        });

    // Reader thread: the sole reader, hammering the stable slot throughout growth.
    std::thread reader(
        [&lane, &stop]
        {
            lane.bind_reader_to_current_thread();
            for (int i = 0; i < 200000; ++i)
            {
                ASSERT_TRUE(lane.valid(0, 1));
                ASSERT_EQ(lane.at(0)->verts, 4242);
            }
            stop.store(true, std::memory_order_relaxed);
        });

    reader.join();
    grower.join();

    // Back on the main thread for the final reads -> re-stamp the reader affinity.
    lane.bind_reader_to_current_thread();
    EXPECT_TRUE(lane.valid(0, 1));
    EXPECT_EQ(lane.at(0)->verts, 4242);
}

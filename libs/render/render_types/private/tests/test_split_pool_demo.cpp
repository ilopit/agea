// ---------------------------------------------------------------------------
// PROTOCOL TESTS for the split pool (laned_storage + lane_allocator).
//
// The implementation graduated to utils/laned_pool.h; this file keeps
// the protocol pinned with a stand-in SPSC batch queue:
//   layer 1  queue.quiescent()         lock-free watermark: `issued`
//            (producer-local plain) vs `applied` (consumer-written atomic,
//            ONE release publish per batch). Counts commands CONSUMED, not
//            "succeeded" -- a tolerated stale retire still moves it, so it
//            can never wedge. Legal ONLY from the producer thread.
//   layer 2  consistent(storage, a...) PER LANE: each allocator's live count
//            vs ITS OWN lane's populated count. Meaningful only once layer 1
//            holds (and deferred frees matured); the tests assert
//            quiescent() first, in that order.
// Plus: dispatch-token routing (commands carry their target storage), lane
// claim/release through the queue, claim epochs (cross-claim ABA), the
// forged-handle trust boundary, retire-vs-reset payload semantics.
//
// The engine's real channel is getr_subsystem_queues().render; the queue here is the
// minimal shape that exercises the watermark contract. Single-threaded by
// design -- cross-thread chunk/affinity mechanics live in test_handle.
// ---------------------------------------------------------------------------

#include <utils/laned_pool.h>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace kryga::utils;  // laned_storage / lane_allocator / handle / slot_state

namespace
{

constexpr uint8_t KIND_DEMO = 7;
using demo_handle = handle<KIND_DEMO>;

// --- render-side verbs over the laned storage --------------------------------
// The loader implements these per resource type; the generic shapes here pin
// the contract.

// [render thread] Fill a reserved slot and mirror its generation
// (== loader populate_*). Growth happens HERE, consumer-side: the populate
// command is what makes the index addressable, so growth rides the SPSC
// queue like every other storage mutation.
template <uint8_t Kind, typename T>
T*
populate(laned_storage<Kind, T>& s, handle<Kind> h, T value)
{
    s.grow_for(h);
    auto* slot = s.at(h);
    *slot = std::move(value);
    s.set_generation(h, h.generation());
    return slot;
}

// [render thread] Handle -> payload; null when empty/stale (== loader get_*).
template <uint8_t Kind, typename T>
T*
get(laned_storage<Kind, T>& s, handle<Kind> h)
{
    return s.valid(h) ? s.at(h) : nullptr;
}

// [render thread] Mark empty, payload left in place for in-flight frames
// (== loader retire_*). false on stale/null: caller picks the policy
// (objects assert, lights tolerate).
template <uint8_t Kind, typename T>
bool
retire(laned_storage<Kind, T>& s, handle<Kind> h)
{
    if (!s.valid(h))
    {
        return false;
    }
    s.set_generation(h, 0);
    return true;
}

// ---------------------------------------------------------------------------
// spsc_batch_queue -- stand-in for the engine's SPSC command channel, batch-
// granular like the real one. What this file pins down is the WATERMARK
// protocol:
//   producer: enqueue() stages, publish() seals a batch, `issued` += n
//             (plain uint64 -- producer-local, the consumer never reads it)
//   consumer: drain_batch() runs one batch, then bumps `applied` ONCE
//             (release) -- the watermark moves per batch, not per command
//   producer: quiescent() acquires `applied`, compares to `issued`. Exact
//             from the producer thread: only it can move `issued`, and
//             `applied` can only catch up.
// The acquire that observes applied == issued synchronizes-with the batch's
// release, so every storage write those commands made is visible -- that is
// what licenses the layer-2 read of the lane counters afterwards.
//
// Production carrier notes (this std::function/deque shape is a stand-in):
//   - commands become tagged PODs in a ring with arena payloads; the
//     type-erased closures here heap-allocate past the SBO and their frees
//     land on the CONSUMER thread (cross-thread heap traffic every frame);
//   - the ring must carry explicit batch markers: `applied` advances per
//     BATCH, and that boundary has to survive into the real channel;
//   - staging is moved-from each publish (capacity re-grown every frame);
//     production double-buffers / recycles batch buffers.
// ---------------------------------------------------------------------------
struct spsc_batch_queue
{
    using cmd = std::function<void()>;

    // producer (model) side
    std::vector<cmd> staging;
    uint64_t issued = 0;

    // channel + consumer (render) side. `applied` gets its own cache line:
    // its release store + the producer's quiescent() poll must not ping-pong
    // the line the producer is mutating on every enqueue.
    std::deque<std::vector<cmd>> batches;
    alignas(64) std::atomic<uint64_t> applied{0};

    void
    enqueue(cmd c)
    {
        staging.push_back(std::move(c));
    }

    // Seal staged commands into one batch (== end-of-frame flush).
    void
    publish()
    {
        if (staging.empty())
        {
            return;
        }
        issued += staging.size();
        batches.push_back(std::move(staging));
        staging.clear();
    }

    // Consume ONE batch. Every consumed command counts -- including ones whose
    // body early-outs (stale retire) -- so the watermark can never wedge.
    // noexcept is the other half of that guarantee: a THROWING command would
    // drop the rest of the batch and leave `applied` behind forever. Commands
    // are noexcept by contract; a violation terminates loudly instead.
    void
    drain_batch() noexcept
    {
        KRG_check(!batches.empty(), "drain_batch on an empty channel");
        auto batch = std::move(batches.front());
        batches.pop_front();
        for (auto& c : batch)
        {
            c();
        }
        applied.store(applied.load(std::memory_order_relaxed) + batch.size(),
                      std::memory_order_release);
    }

    void
    drain_all()
    {
        while (!batches.empty())
        {
            drain_batch();
        }
    }

    // [producer thread ONLY] Layer-1 check. `issued` is a plain u64: calling
    // this from any other thread is a data race, not an "inexact" read.
    bool
    quiescent() const
    {
        return applied.load(std::memory_order_acquire) == issued;
    }
};

// --- model-side verbs ---------------------------------------------------------
// This is what the dispatch token buys: the producer call site needs ONLY the
// allocator and the queue. The command carries its own target storage, so one
// queue can interleave commands for many storages and the consumer just runs
// them -- no routing table render-side.

// [model thread] Two-phase create: mint the identity now, queue the populate.
template <uint8_t Kind, typename T>
handle<Kind>
create(spsc_batch_queue& q, lane_allocator<Kind, T>& a, T value)
{
    auto h = a.reserve();
    q.enqueue([s = a.storage(), h, v = std::move(value)]() mutable
              { populate(*s, h, std::move(v)); });
    return h;
}

// [model thread] Deferred destroy: the identity dies now, the retire lands at
// drain, the index matures via tick().
template <uint8_t Kind, typename T>
void
destroy(spsc_batch_queue& q, lane_allocator<Kind, T>& a, handle<Kind> h)
{
    a.free(h);
    q.enqueue([s = a.storage(), h] { retire(*s, h); });
}

// Payload with observable lifetime: the shared_ptr token distinguishes
// "payload still alive in the slot" (use_count) from "destructed".
struct probe
{
    int value = 0;
    std::shared_ptr<int> token;
};

using demo_storage = laned_storage<KIND_DEMO, probe>;
using demo_allocator = lane_allocator<KIND_DEMO, probe>;

}  // namespace

// --- Watermark: two-phase create gated by quiescent() ------------------------
// The halves are separate locals; the only wiring is the dispatch token in
// the allocator ctor. Note the create() call site: no storage argument --
// the command routes itself. Between publish and drain the watermark says
// "in flight"; that, not the storage state, is what the model thread polls.
TEST(split_pool_demo, watermark_two_phase_create)
{
    demo_storage storage{1};
    demo_allocator alloc{storage, 0};
    spsc_batch_queue q;

    auto h = create(q, alloc, {.value = 7, .token = nullptr});

    // Staged but not published: nothing issued yet, queue is trivially quiescent.
    EXPECT_TRUE(q.quiescent());

    q.publish();

    // In-flight window: identity exists, payload doesn't, watermark diverged.
    EXPECT_FALSE(q.quiescent());
    EXPECT_TRUE(alloc.valid(h));
    EXPECT_EQ(get(storage, h), nullptr);
    EXPECT_FALSE(consistent(storage, alloc));  // reserved=1 vs populated=0

    q.drain_all();

    // Layered check, in order: watermark first, then the strong invariant.
    ASSERT_TRUE(q.quiescent());
    EXPECT_TRUE(consistent(storage, alloc));
    ASSERT_NE(get(storage, h), nullptr);
    EXPECT_EQ(get(storage, h)->value, 7);

    alloc.detach();  // single-threaded test == storage-owner thread
}

// --- Watermark moves per BATCH, not per command -------------------------------
TEST(split_pool_demo, watermark_is_batch_granular)
{
    demo_storage storage{1};
    demo_allocator alloc{storage, 0};
    spsc_batch_queue q;

    // Batch A: two creates. Batch B: one create.
    create(q, alloc, {.value = 1, .token = nullptr});
    create(q, alloc, {.value = 2, .token = nullptr});
    q.publish();

    create(q, alloc, {.value = 3, .token = nullptr});
    q.publish();

    EXPECT_EQ(q.issued, 3u);
    EXPECT_EQ(q.applied.load(), 0u);

    q.drain_batch();  // batch A lands as ONE watermark step of 2
    EXPECT_EQ(q.applied.load(), 2u);
    EXPECT_FALSE(q.quiescent());  // batch B still in flight

    q.drain_batch();
    ASSERT_TRUE(q.quiescent());
    EXPECT_TRUE(consistent(storage, alloc));

    alloc.detach();
}

// --- Deferred destroy: divergence window closes at the watermark --------------
// Model frees the identity now (no new references), the retire command lands
// at drain -- in between, in-flight frames still resolve the payload. The
// index recycles only after the deferral matures.
TEST(split_pool_demo, deferred_destroy_eventual_consistency)
{
    demo_storage storage{1};
    demo_allocator alloc{storage, 0};
    spsc_batch_queue q;
    alloc.set_defer_ticks(3);  // == frames in flight

    auto h = create(q, alloc, {.value = 1, .token = nullptr});
    q.publish();
    q.drain_all();
    ASSERT_TRUE(q.quiescent());
    ASSERT_TRUE(consistent(storage, alloc));

    // Model thread: destroy -> free identity now, retire lands at drain.
    destroy(q, alloc, h);
    q.publish();

    // Divergence window: layer 1 already reports it ("not quiescent"), so the
    // layer-2 disagreement is EXPECTED, not an error.
    EXPECT_FALSE(q.quiescent());
    EXPECT_FALSE(alloc.valid(h));         // no new reference can be made
    EXPECT_NE(get(storage, h), nullptr);  // render side still resolves it
    EXPECT_FALSE(consistent(storage, alloc));

    q.drain_all();
    ASSERT_TRUE(q.quiescent());
    EXPECT_TRUE(consistent(storage, alloc));
    EXPECT_EQ(get(storage, h), nullptr);

    // Index recycling waits for the deferral window, NOT the watermark.
    auto early = alloc.reserve();
    EXPECT_NE(early.index(), h.index());

    alloc.tick();
    alloc.tick();
    alloc.tick();  // defer_ticks elapsed -> slot matured

    auto recycled = alloc.reserve();
    EXPECT_EQ(recycled.index(), h.index());
    EXPECT_NE(recycled.generation(), h.generation());

    alloc.detach();
}

// --- Retire leaves the payload in place; reset destructs it -------------------
// The engine distinction: objects/lights RETIRE (in-flight frames hold raw
// pointers into the slot -- the data must survive until the deferred free
// drains), content meshes RESET (GPU buffer refs released right there).
TEST(split_pool_demo, retire_keeps_payload_reset_destructs)
{
    demo_storage storage{1};
    demo_allocator alloc{storage, 0};

    auto token = std::make_shared<int>(0);

    auto h = alloc.reserve();
    auto* slot = populate(storage, h, {.value = 5, .token = token});
    ASSERT_EQ(token.use_count(), 2);  // test + slot

    EXPECT_TRUE(retire(storage, h));
    EXPECT_EQ(get(storage, h), nullptr);
    EXPECT_EQ(token.use_count(), 2);  // payload NOT destructed
    EXPECT_EQ(slot->value, 5);        // raw pointer still reads the old data

    storage.reset(h);  // destruct in place -- the token drops
    EXPECT_EQ(token.use_count(), 1);

    alloc.reclaim(h);
    EXPECT_TRUE(consistent(storage, alloc));

    alloc.detach();
}

// --- Stale retire is consumed, reported, and does NOT wedge the watermark -----
// The watermark counts consumed commands; retire's bool stays the per-call
// policy hook (lights tolerate, objects assert).
TEST(split_pool_demo, stale_retire_still_moves_watermark)
{
    demo_storage storage{1};
    demo_allocator alloc{storage, 0};
    spsc_batch_queue q;

    auto h = alloc.reserve();
    populate(storage, h, {.value = 3, .token = nullptr});

    EXPECT_TRUE(retire(storage, h));
    EXPECT_FALSE(retire(storage, h));              // stale: caller's policy call
    EXPECT_FALSE(retire(storage, demo_handle{}));  // null: same contract

    // A stale retire arriving through the queue still counts as applied. The
    // command dispatches off the allocator's token, like destroy() builds it.
    q.enqueue([s = alloc.storage(), h] { EXPECT_FALSE(retire(*s, h)); });
    q.publish();
    q.drain_all();
    EXPECT_TRUE(q.quiescent());

    alloc.detach();
}

// --- Forged handles bounce off the trust-boundary gate -------------------------
// Handles are raw u64s when they cross RPC / save data / a memory stomp; the
// validity gate cannot rely on the static typing. The two holes this pins:
// gen-0 forgery (matches any empty/retired slot's shadow) and a wrong kind
// tag riding an otherwise-plausible bit pattern.
TEST(split_pool_demo, forged_handles_rejected)
{
    demo_storage storage{1};
    demo_allocator alloc{storage, 0};

    auto h = alloc.reserve();
    populate(storage, h, {.value = 1, .token = nullptr});
    EXPECT_TRUE(retire(storage, h));  // retired: shadow gen 0, payload parked in place

    // Gen-0 forgery: non-null (kind bits set), in-range index, generation 0.
    // Without the gate it compares 0 == 0 against the retired slot's shadow
    // and get() hands out the parked payload.
    demo_handle gen0{};
    gen0.v = (uint64_t(KIND_DEMO) << (demo_handle::index_bits + demo_handle::gen_bits)) |
             h.index();
    ASSERT_TRUE(gen0);  // it IS non-null -- that's what makes it dangerous
    EXPECT_FALSE(storage.valid(gen0));
    EXPECT_EQ(get(storage, gen0), nullptr);
    EXPECT_FALSE(retire(storage, gen0));

    // Kind confusion: correct index + generation, wrong kind tag. The static
    // handle type can't catch a reinterpreted u64; the runtime compare does.
    auto h2 = alloc.reserve();
    populate(storage, h2, {.value = 2, .token = nullptr});
    demo_handle wrong_kind{};
    wrong_kind.v =
        (uint64_t(KIND_DEMO + 1) << (demo_handle::index_bits + demo_handle::gen_bits)) |
        (uint64_t(h2.generation()) << demo_handle::index_bits) | h2.index();
    EXPECT_FALSE(storage.valid(wrong_kind));
    EXPECT_FALSE(alloc.owns(wrong_kind));
    EXPECT_FALSE(alloc.valid(wrong_kind));
    EXPECT_EQ(get(storage, wrong_kind), nullptr);

    // The real handle still resolves: the gate rejects forgeries, not traffic.
    EXPECT_NE(get(storage, h2), nullptr);

    alloc.detach();
}

// --- Detach rides the queue; quiescence licenses the re-claim ------------------
// Cross-thread allocator teardown without a single unsynchronized write: the
// lane release is just another command (FIFO puts it after the allocator's
// last retire), and the next claimant waits for quiescent() -- the same
// watermark acquire that licenses consistent() orders claim-after-release.
// The release also bumps the lane EPOCH: the second half of the test is the
// cross-claim ABA that epochs exist to kill.
TEST(split_pool_demo, detach_rides_the_queue)
{
    demo_storage storage{1};
    spsc_batch_queue q;

    demo_handle leaked{};  // a stale handle that "escapes" the first owner
    {
        demo_allocator alloc{storage, 0};
        auto h = create(q, alloc, {.value = 1, .token = nullptr});
        leaked = h;
        destroy(q, alloc, h);  // identity dead, retire queued
        alloc.detach(q);       // release queued, FIFO after the retire
        q.publish();
        // The allocator object can die right here: every queued command
        // carries the dispatch token by VALUE, nothing references it.
    }

    // Release still in flight: the lane is NOT re-claimable yet, and the
    // watermark is what says so.
    EXPECT_FALSE(q.quiescent());

    q.drain_all();
    ASSERT_TRUE(q.quiescent());  // <- this acquire licenses the claim below

    demo_allocator next{storage, 0};  // re-claim: would assert had the
                                      // release not drained first
    EXPECT_EQ(next.active_count(), 0u);
    EXPECT_TRUE(consistent(storage, next));

    // Cross-claim ABA: the new owner re-mints the SAME index with a counter
    // restarting at 1 -- exactly the bit pattern the leaked handle carried.
    // Without epochs, populate would re-validate it against the new payload.
    auto h2 = create(q, next, {.value = 2, .token = nullptr});
    q.publish();
    q.drain_all();
    ASSERT_EQ(h2.index(), leaked.index());
    EXPECT_EQ(leaked.generation() & k_gen_counter_mask, uint32_t(1));  // same counter...
    EXPECT_NE(h2.generation(), leaked.generation());                   // ...different epoch
    EXPECT_FALSE(storage.valid(leaked));
    EXPECT_EQ(get(storage, leaked), nullptr);
    EXPECT_EQ(get(storage, h2)->value, 2);

    next.detach();
}

// --- One storage, two allocators on separate lanes -----------------------------
// The engine shape this probes: ONE mesh storage; the renderer's system
// allocator owns lane 0 and works same-thread, render_translator's content
// allocator owns lane 1 and works through the queue. The draw path gets a
// single lookup entry point; the producers never touch each other's state.
TEST(split_pool_demo, shared_storage_two_lanes)
{
    demo_storage storage{2};
    demo_allocator system_alloc{storage, 0};
    demo_allocator content_alloc{storage, 1};
    spsc_batch_queue q;

    // System pool: same-thread create (reserve + populate, no queue).
    auto sh = system_alloc.reserve();
    populate(storage, sh, {.value = 1, .token = nullptr});

    // Content pool: two-phase create through the queue.
    auto ch = create(q, content_alloc, {.value = 2, .token = nullptr});
    q.publish();

    // Lane id rides in the index top bits: no collision by construction.
    EXPECT_EQ(sh.index(), 0u);
    EXPECT_EQ(ch.index(), uint32_t(1) << k_lane_shift);

    // Cross-lane handles are foreign to the sibling ALLOCATOR...
    EXPECT_FALSE(content_alloc.owns(sh));
    EXPECT_FALSE(system_alloc.owns(ch));
    EXPECT_FALSE(content_alloc.valid(sh));
    // ...but the shared STORAGE resolves both (single draw-path entry point).
    EXPECT_NE(get(storage, sh), nullptr);

    q.drain_all();
    ASSERT_TRUE(q.quiescent());
    EXPECT_TRUE(consistent(storage, system_alloc, content_alloc));
    EXPECT_EQ(get(storage, ch)->value, 2);

    // System destroy: reset + reclaim, synchronous; per-lane invariants hold
    // with the content side untouched.
    storage.reset(sh);
    system_alloc.reclaim(sh);
    EXPECT_TRUE(consistent(storage, system_alloc, content_alloc));
    EXPECT_EQ(storage.active(), 1u);

    // Lanes recycle independently: the system index returns to ITS free-list.
    auto sh2 = system_alloc.reserve();
    EXPECT_EQ(sh2.index(), 0u);
    EXPECT_NE(sh2.generation(), sh.generation());

    system_alloc.detach();
    content_alloc.detach();
}

// --- Quiescence invariant over a mixed two-lane workload ----------------------
// The check the watermark + lanes enable: arbitrary create/destroy mix,
// system lane synchronous, content lane batched; after the queue drains and
// deferred frees mature, layer 2 MUST hold on EVERY lane.
TEST(split_pool_demo, quiescence_invariant_mixed_workload)
{
    demo_storage storage{2};
    demo_allocator system_alloc{storage, 0};
    demo_allocator content_alloc{storage, 1};
    spsc_batch_queue q;
    content_alloc.set_defer_ticks(2);

    std::vector<demo_handle> live;
    demo_handle sys_live{};
    for (int round = 0; round < 5; ++round)
    {
        // Content: create a few through the queue.
        for (int i = 0; i < 4; ++i)
        {
            live.push_back(create(q, content_alloc, {.value = i, .token = nullptr}));
        }
        // Content: destroy every other live handle.
        for (size_t i = 0; i + 1 < live.size(); i += 2)
        {
            destroy(q, content_alloc, live[i]);
            live.erase(live.begin() + static_cast<ptrdiff_t>(i));
        }
        // System: churn one synchronous slot alongside.
        if (sys_live)
        {
            storage.reset(sys_live);
            system_alloc.reclaim(sys_live);
        }
        sys_live = system_alloc.reserve();
        populate(storage, sys_live, {.value = round, .token = nullptr});

        q.publish();
        q.drain_all();  // frame boundary: render consumes the batch
        content_alloc.tick();
    }

    // Mature everything still parked.
    content_alloc.tick();
    content_alloc.tick();

    ASSERT_TRUE(q.quiescent());
    EXPECT_TRUE(consistent(storage, system_alloc, content_alloc))
        << "system=" << system_alloc.active_count() << "/" << storage.lane_active(0)
        << " content=" << content_alloc.active_count() << "/" << storage.lane_active(1);
    EXPECT_EQ(storage.active(), live.size() + 1);  // +1 system slot
    for (auto h : live)
    {
        EXPECT_NE(get(storage, h), nullptr);
    }

    system_alloc.detach();
    content_alloc.detach();
}

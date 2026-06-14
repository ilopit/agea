#pragma once

// ---------------------------------------------------------------------------
// Split render-resource pool: laned_storage (render side) + lane_allocator
// (model side). Production port of the design probe in test_split_pool_demo.cpp
// (which keeps the protocol tests).
//
// The split:
//   - lane_allocator mints identities (index + generation) and NEVER calls
//     into the storage in steady state. It holds the storage pointer as a
//     DISPATCH TOKEN: model-side code copies it into queued commands so the
//     render side knows which storage each command targets.
//   - laned_storage holds the payloads. One slot LANE (independent chunked
//     slot_storage) per allocator; every handle carries its lane id in the
//     top bits of the index, so handles self-route at the storage and index
//     collisions between allocators are impossible by construction.
//   - growth is CONSUMER-side: populate paths call grow_for(h) at execute
//     time, so the grower == the reader == the render thread and the
//     cross-thread growth race disappears. The exceptions are init-time
//     (preallocate -> grow_lane, single-threaded by definition).
//
// Lane ownership protocol (all touches ordered, claim flag needs no atomics):
//   claim    allocator ctor; legal pre-threading, or as a RE-claim after the
//            previous owner's release drained and quiescent() was observed.
//   release  rides the command queue via detach(queue) -- FIFO puts it after
//            every command the departing allocator issued. The no-arg
//            detach() is for allocators living on the storage's own thread.
//   epoch    release bumps the lane's claim epoch; the next claimant stamps
//            it into the top bits of every generation it mints, so stale
//            handles from a previous owner can never re-validate (cross-claim
//            ABA is structurally impossible).
//
// Teardown order: free everything -> detach -> publish -> drain ->
// quiescent() -> destroy allocator objects -> destroy storage. The dtor and
// storage asserts catch a skipped step.
// ---------------------------------------------------------------------------

#include <render_types/handle_pool.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

// Thread-ownership guards behind a dedicated switch: when off, the affinity
// member, every check call and the message literals compile out entirely.
// Defaults follow NDEBUG; the switch is independent so a release build can
// keep guards while soak-testing threading changes.
#ifndef KRG_POOL_THREAD_GUARDS
#ifdef NDEBUG
#define KRG_POOL_THREAD_GUARDS 0
#else
#define KRG_POOL_THREAD_GUARDS 1
#endif
#endif

#if KRG_POOL_THREAD_GUARDS
#define KRG_pool_affinity(name) kryga::render::types::detail::thread_affinity name
#define KRG_pool_affinity_check(guard, what) (guard).check(what)
#define KRG_pool_affinity_bind(guard) (guard).bind()
#else
#define KRG_pool_affinity(name)
#define KRG_pool_affinity_check(guard, what) ((void)0)
#define KRG_pool_affinity_bind(guard) ((void)0)
#endif

namespace kryga
{
namespace render
{
namespace types
{

// Lane addressing: the top 3 bits of the 24-bit index select the lane ->
// up to 8 allocators per storage, 2M slot ids each.
constexpr uint32_t k_lane_bits = 3;
constexpr uint32_t k_lane_count = uint32_t(1) << k_lane_bits;            // 8
constexpr uint32_t k_lane_shift = handle<0>::index_bits - k_lane_bits;   // 21
constexpr uint32_t k_lane_local_mask = (uint32_t(1) << k_lane_shift) - 1;

constexpr uint32_t
lane_of(uint32_t index)
{
    return index >> k_lane_shift;
}
constexpr uint32_t
local_of(uint32_t index)
{
    return index & k_lane_local_mask;
}

// Claim-EPOCH split of the 32-bit generation: the top bits record WHICH claim
// of the lane minted a generation (see the lane ownership protocol above).
// Budget 8/24: 256 claims per lane (claims are level reloads; release asserts
// before the epoch wraps) and 16M frees per index within one claim.
constexpr uint32_t k_epoch_bits = 8;
constexpr uint32_t k_epoch_count = uint32_t(1) << k_epoch_bits;  // 256
constexpr uint32_t k_gen_counter_bits = uint32_t(handle<0>::gen_bits) - k_epoch_bits;  // 24
constexpr uint32_t k_gen_counter_mask = (uint32_t(1) << k_gen_counter_bits) - 1;

// ---------------------------------------------------------------------------
// laned_storage -- ONE storage object, one slot vector (lane) per allocator.
// Render-thread owned in steady state; see the protocol above for the only
// model-side touches (claim at ctor time, grow_lane at preallocate time).
// Lane count is a RUNTIME choice (one lane per allocator the storage serves).
// ---------------------------------------------------------------------------
template <uint8_t Kind, typename T>
class laned_storage
{
public:
    using handle_t = handle<Kind>;

    // A lane's local index space is 2^21, but the backing slot_storage caps
    // at k_default_max_slots (~1M) -- the SMALLER of the two is the real
    // capacity. Allocators assert reserve() against this.
    static constexpr uint32_t k_lane_capacity =
        std::min<uint32_t>(k_lane_local_mask + 1, slot_storage<Kind, T>::k_default_max_slots);

    explicit laned_storage(uint32_t lane_count)
        : m_lane_count(lane_count)
        , m_lanes(std::make_unique<slot_storage<Kind, T>[]>(lane_count))
    {
        KRG_check(lane_count >= 1 && lane_count <= k_lane_count,
                  "lane count must fit the handle's lane bits");
    }

    ~laned_storage()
    {
        // Pins the teardown order: allocators (which release their claim via
        // detach) must die first -- their queued dispatch tokens point here.
        for (uint32_t i = 0; i < m_lane_count; ++i)
        {
            KRG_check(!m_claimed[i], "storage destroyed with an allocator still attached");
        }
    }

    uint32_t
    lane_count() const
    {
        return m_lane_count;
    }

    // --- lane ownership (see the protocol in the file header) ----------------

    // Returns the lane's claim epoch for the new owner to mint under.
    uint32_t
    claim_lane(uint32_t i)
    {
        KRG_check(i < m_lane_count, "lane id out of the configured range");
        KRG_check(!m_claimed[i],
                  "lane already claimed -- second allocator, or re-claim before quiescent()");
        m_claimed[i] = true;
        return m_epochs[i];
    }

    void
    release_lane(uint32_t i)
    {
        KRG_check(i < m_lane_count && m_claimed[i], "releasing an unclaimed lane");
        m_claimed[i] = false;
        ++m_epochs[i];
        // Wrap would let a 256-claims-old handle alias the new epoch. Claims
        // are level reloads; hitting this means something else is broken.
        KRG_check(m_epochs[i] < k_epoch_count, "lane claim epoch exhausted");
    }

    // --- [render thread] reads/writes; the handle self-routes ----------------

    // Make the slot behind h addressable (chunk allocated, zeroed). Called by
    // populate paths at execute time: growth is CONSUMER-side.
    void
    grow_for(handle_t h)
    {
        route(h).grow_to(local_of(h.index()) + 1);
    }

    // [init, single-threaded] Bulk pre-growth driven by an allocator's
    // preallocate -- the sanctioned non-render-side growth (no cross-thread
    // traffic exists yet).
    void
    grow_lane(uint32_t lane, uint32_t count)
    {
        KRG_check(lane < m_lane_count, "lane id out of the configured range");
        m_lanes[lane].grow_to(count);
    }

    T*
    at(handle_t h)
    {
        return route(h).at(local_of(h.index()));
    }

    bool
    valid(handle_t h) const
    {
        // TRUST-BOUNDARY gate: a handle here may be forged or corrupted (RPC,
        // save data, a stomp reinterpreted as a handle). The kind compare
        // backs the static typing for raw u64s; the gen-0 gate closes what
        // slot_storage::valid lets through (a forged gen-0 handle compares
        // EQUAL to the shadow gen of any empty/retired slot). A lane past the
        // configured count is just "not a slot here" -- query semantics.
        return h && h.kind() == Kind && h.generation() != 0 &&
               lane_of(h.index()) < m_lane_count &&
               m_lanes[lane_of(h.index())].valid(local_handle(h));
    }

    void
    set_generation(handle_t h, uint32_t generation)
    {
        // slot_storage trusts its index (no bounds check, even in release);
        // gate here so a corrupt index is a clean assert, not a write through
        // an out-of-range chunk-table read.
        auto& lane = route(h);
        KRG_check(local_of(h.index()) < lane.size(), "set_generation past the populated range");
        lane.set_generation(local_of(h.index()), generation);
    }

    void
    reset(handle_t h)
    {
        auto& lane = route(h);
        KRG_check(local_of(h.index()) < lane.size(), "reset past the populated range");
        lane.reset(local_of(h.index()));
    }

    // [render thread] Raw per-lane access for iteration and GPU-slot-indexed
    // APIs (the lane's slot_storage works in LOCAL index space).
    slot_storage<Kind, T>&
    lane(uint32_t i)
    {
        KRG_check(i < m_lane_count, "lane id out of the configured range");
        return m_lanes[i];
    }

    const slot_storage<Kind, T>&
    lane(uint32_t i) const
    {
        KRG_check(i < m_lane_count, "lane id out of the configured range");
        return m_lanes[i];
    }

    // Per-lane live counter -- what layer 2 checks an allocator against.
    uint64_t
    lane_active(uint32_t i) const
    {
        KRG_check(i < m_lane_count, "lane id out of the configured range");
        return m_lanes[i].active();
    }

    uint64_t
    active() const
    {
        uint64_t sum = 0;
        for (uint32_t i = 0; i < m_lane_count; ++i)
        {
            sum += m_lanes[i].active();
        }
        return sum;
    }

    // Addressable slots summed over lanes (logging/stats; for a single-lane
    // storage this IS the lane's size).
    uint64_t
    size() const
    {
        uint64_t sum = 0;
        for (uint32_t i = 0; i < m_lane_count; ++i)
        {
            sum += m_lanes[i].size();
        }
        return sum;
    }

    // Re-stamp every lane's thread bindings to the calling thread at a
    // lifecycle handoff (init thread populated, then the steady-state consumer
    // thread takes over; main reclaims at teardown). Storage access is
    // single-consumer across ALL lanes -- populate/at/reset/grow_for run on
    // the one consumer thread regardless of which producer minted the handle.
    void
    bind_to_current_thread()
    {
        for (uint32_t i = 0; i < m_lane_count; ++i)
        {
            m_lanes[i].bind_to_current_thread();
        }
    }

    // [teardown, single-threaded] Free every lane's chunks. Claims and epochs
    // survive: clearing storage is a payload reset, not a lane handoff.
    void
    clear()
    {
        for (uint32_t i = 0; i < m_lane_count; ++i)
        {
            m_lanes[i].clear();
        }
    }

private:
    slot_storage<Kind, T>&
    route(handle_t h)
    {
        KRG_check(lane_of(h.index()) < m_lane_count, "handle lane out of the configured range");
        return m_lanes[lane_of(h.index())];
    }

    // The lane's slot_storage works in LOCAL index space; strip the lane bits.
    static handle_t
    local_handle(handle_t h)
    {
        return handle_t::make(local_of(h.index()), h.generation());
    }

    uint32_t m_lane_count;
    std::unique_ptr<slot_storage<Kind, T>[]> m_lanes;
    bool m_claimed[k_lane_count] = {};
    uint32_t m_epochs[k_lane_count] = {};  // bumped per release; see claim_lane
};

// ---------------------------------------------------------------------------
// lane_allocator -- the model-side half: free-list + generation table +
// per-slot residency state + deferred-free parking matured by tick().
// Constructed against ONE storage + ONE lane; the storage pointer is a
// DISPATCH TOKEN (see file header), never called through in steady state.
// Single-owner: every method runs on the one owning (producer) thread.
// ---------------------------------------------------------------------------
template <uint8_t Kind, typename T>
class lane_allocator
{
public:
    using handle_t = handle<Kind>;
    using storage_t = laned_storage<Kind, T>;

    // NULLABLE: default-constructed it is unbound (no storage, no claim) --
    // holders created before their storage exists (render_translator under
    // global-state init order) sit unbound until bind(). Every minting path
    // asserts boundness; bound() is the explicit query.
    lane_allocator() = default;

    lane_allocator(storage_t& storage, uint32_t lane)
    {
        bind(storage, lane);
    }

    ~lane_allocator()
    {
        // NO release here: a dtor can run on the model thread at any moment,
        // and a direct claim-flag write from it would race the render side.
        // Release goes through detach(); this assert makes a forgotten
        // detach a loud teardown failure instead of silent corruption.
        KRG_check(!m_storage, "allocator destroyed while still attached -- detach() first");
    }

    // The claim makes this the lane's unique owner; copies are meaningless.
    lane_allocator(const lane_allocator&) = delete;
    lane_allocator&
    operator=(const lane_allocator&) = delete;

    bool
    bound() const
    {
        return m_storage != nullptr;
    }

    // Re-stamp single-owner affinity at a thread handoff: the init thread
    // binds/preallocates, then the steady-state owner (e.g. the render thread
    // for the renderer's system allocators) calls this once before its first
    // touch. Purely a debug-guard operation -- compiles out with the guards.
    void
    bind_to_current_thread()
    {
        KRG_pool_affinity_bind(m_affinity);
    }

    // [init / post-quiescence re-claim] Claim a lane and become its owner.
    // detach() left the bookkeeping pristine, so minting starts fresh under
    // the lane's NEW epoch -- nothing from a previous binding leaks through.
    void
    bind(storage_t& storage, uint32_t lane)
    {
        KRG_check(!m_storage, "allocator already bound -- detach() first");
        KRG_check(lane < storage.lane_count(), "lane id outside the storage's configured lanes");
        m_storage = &storage;
        m_lane = lane;
        // Pre-shift this claim's epoch once; mint with a cheap OR ever after.
        m_epoch_base = storage.claim_lane(lane) << k_gen_counter_bits;
    }

    // [producer thread] Cross-thread detach: the lane release RIDES THE QUEUE
    // like every other render-side mutation; FIFO puts it after every command
    // this allocator ever issued. The lane's next claimant must observe queue
    // quiescence first. The allocator returns to the pristine unbound state:
    // every identity dies with the claim (full-teardown semantics -- the
    // storage is cleared wholesale alongside), and any handle that survives
    // anyway is staled by the epoch bump.
    template <typename Queue>
    void
    detach(Queue& q)
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::detach from a non-owner thread");
        KRG_check(m_storage, "detach of an unbound allocator");
        q.enqueue([s = m_storage, lane = m_lane] { s->release_lane(lane); });
        unbind();
    }

    // [storage-owner thread / single-threaded teardown] Direct detach, for
    // allocators living on the same thread as the storage -- exactly the
    // contexts where calling the storage is already legal.
    void
    detach()
    {
        KRG_check(m_storage, "detach of an unbound allocator");
        m_storage->release_lane(m_lane);
        unbind();
    }

    // The dispatch token. Render-side code dereferences it at execute time;
    // model-side code only copies it into commands.
    storage_t*
    storage() const
    {
        return m_storage;
    }

    uint32_t
    lane() const
    {
        return m_lane;
    }

    // [init, single-threaded] Pre-grow to n free slots in one shot: fills the
    // bookkeeping, free-lists 0..n-1 (ascending hand-out) and grows the
    // storage lane DIRECTLY -- the sanctioned init-time growth, before any
    // cross-thread traffic exists. Call once on a fresh allocator.
    void
    preallocate(uint32_t n)
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::preallocate from a non-owner thread");
        KRG_check(m_storage, "preallocate on an unbound allocator");
        KRG_check(n <= storage_t::k_lane_capacity, "preallocate past lane capacity");
        while (m_entries.size() < n)
        {
            m_entries.push_back({m_epoch_base | 1, slot_state::free});
        }
        m_free.clear();
        // Push descending so the back (which reserve() pops first) is index 0.
        for (uint32_t i = n; i-- > 0;)
        {
            m_free.push_back(i);
        }
        m_storage->grow_lane(m_lane, n);
    }

    handle_t
    reserve()
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::reserve from a non-owner thread");
        KRG_check(m_storage, "reserve on an unbound allocator");
        uint32_t idx;
        if (!m_free.empty())
        {
            idx = m_free.back();
            m_free.pop_back();
        }
        else
        {
            idx = uint32_t(m_entries.size());
            KRG_check(idx < storage_t::k_lane_capacity, "lane capacity exhausted");
            m_entries.push_back({m_epoch_base | 1, slot_state::free});
        }

        auto& e = m_entries[idx];
        e.state = slot_state::reserved;
        ++m_active;
        return handle_t::make((m_lane << k_lane_shift) | idx, e.generation);
    }

    // Immediate reclaim (system pools: same-thread destroy, nothing in flight).
    void
    reclaim(handle_t h)
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::reclaim from a non-owner thread");
        uint32_t idx = local_checked(h);
        invalidate(idx, slot_state::free);
        m_free.push_back(idx);
    }

    // Deferred free (content pools): the identity dies NOW -- no new reference
    // can be made -- but the index parks until tick() matures it, so in-flight
    // frames never see it recycled.
    void
    free(handle_t h)
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::free from a non-owner thread");
        // Null is "nothing was reserved" -- a legitimate no-op. A stale or
        // foreign handle is a bug and asserts in local_checked.
        if (!h)
        {
            return;
        }
        uint32_t idx = local_checked(h);
        invalidate(idx, slot_state::retiring);
        m_retiring.push_back({idx, m_tick});
    }

    // Advance one frame; return matured indices to the free-list (FIFO).
    void
    tick()
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::tick from a non-owner thread");
        ++m_tick;
        while (m_retire_head < m_retiring.size() &&
               m_retiring[m_retire_head].freed_tick + m_defer_ticks <= m_tick)
        {
            uint32_t idx = m_retiring[m_retire_head].index;
            m_entries[idx].state = slot_state::free;
            m_free.push_back(idx);
            ++m_retire_head;
        }
        // Ring compaction: recycle the buffer once fully drained.
        if (m_retire_head == m_retiring.size())
        {
            m_retiring.clear();
            m_retire_head = 0;
        }
    }

    // Deferral window in frames; drive from frames_in_flight.
    void
    set_defer_ticks(uint64_t n)
    {
        m_defer_ticks = n;
    }

    // Lane membership: a handle from a sibling allocator is FOREIGN here, even
    // though the shared storage resolves it fine. The kind compare backs the
    // static typing at the trust boundary (raw u64s don't respect templates).
    bool
    owns(handle_t h) const
    {
        return h && h.kind() == Kind && lane_of(h.index()) == m_lane;
    }

    bool
    valid(handle_t h) const
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::valid from a non-owner thread");
        if (!owns(h))
        {
            return false;
        }
        // No explicit gen-0 gate needed here: entry generations are never 0,
        // so a forged gen-0 handle can't match. (The STORAGE needs the gate
        // because its empty-slot shadow IS 0.)
        uint32_t idx = local_of(h.index());
        return idx < m_entries.size() && m_entries[idx].generation == h.generation() &&
               m_entries[idx].state != slot_state::free;
    }

    slot_state
    state(handle_t h) const
    {
        KRG_check(valid(h), "state query on a stale or null handle");
        return m_entries[local_of(h.index())].state;
    }

    void
    set_state(handle_t h, slot_state st)
    {
        KRG_check(valid(h), "set_state on a stale or null handle");
        m_entries[local_of(h.index())].state = st;
    }

    uint64_t
    active_count() const
    {
        KRG_pool_affinity_check(m_affinity, "lane_allocator::active_count from a non-owner thread");
        return m_active;
    }

private:
    struct entry
    {
        // Full 32-bit generation: (epoch << k_gen_counter_bits) | counter.
        // Never 0 -- the counter starts at 1 and skips 0 on wrap, so the
        // value is nonzero even under epoch 0 (gen 0 is the null sentinel).
        uint32_t generation = 1;
        slot_state state = slot_state::free;
    };
    struct parked
    {
        uint32_t index;
        uint64_t freed_tick;
    };

    uint32_t
    local_checked(handle_t h) const
    {
        KRG_check(valid(h), "free/reclaim of a stale, null or foreign handle");
        return local_of(h.index());
    }

    // Back to the pristine unbound state (detach): identities die with the
    // claim; the epoch bump stales any handle that survives outside.
    void
    unbind()
    {
        m_storage = nullptr;
        m_entries.clear();
        m_free.clear();
        m_retiring.clear();
        m_retire_head = 0;
        m_active = 0;
        m_tick = 0;
        m_epoch_base = 0;
    }

    // Kill the identity: bump the generation COUNTER (every outstanding
    // handle goes stale) and transition the state. The bump stays inside the
    // counter bits -- the epoch is this owner's identity.
    void
    invalidate(uint32_t idx, slot_state st)
    {
        auto& e = m_entries[idx];
        uint32_t counter = (e.generation + 1) & k_gen_counter_mask;
        if (counter == 0)
        {
            counter = 1;  // skip 0: gen 0 is the null/empty sentinel
        }
        e.generation = m_epoch_base | counter;
        e.state = st;
        --m_active;
    }

    // Null storage IS the null state: unbound until bind(), unbound again
    // after detach(). No separate flag to drift out of sync.
    storage_t* m_storage = nullptr;
    uint32_t m_lane = 0;
    // vector, NOT deque: MSVC deque blocks are 16 bytes -- 1-2 elements per
    // heap allocation, double indirection per access. Nothing holds pointers
    // into these (every call re-indexes by value).
    std::vector<entry> m_entries;
    std::vector<uint32_t> m_free;
    // Strict-FIFO retire queue as a flat ring: head cursor instead of
    // pop_front, buffer recycled when fully drained (see tick).
    std::vector<parked> m_retiring;
    size_t m_retire_head = 0;
    uint64_t m_active = 0;
    uint64_t m_tick = 0;
    uint64_t m_defer_ticks = 3;  // == frames in flight; set from the device
    uint32_t m_epoch_base = 0;   // this claim's epoch, pre-shifted (see bind)

    KRG_pool_affinity(m_affinity);  // the single owning (producer) thread
};

// Layer-2 consistency invariant, PER LANE: each allocator's live identities
// == its own lane's populated slots. Only meaningful at queue quiescence with
// deferred frees matured; callers assert the layer-1 watermark first.
template <uint8_t Kind, typename T, typename... Alloc>
bool
consistent(const laned_storage<Kind, T>& s, const Alloc&... alloc)
{
    return ((alloc.active_count() == s.lane_active(alloc.lane())) && ...);
}

}  // namespace types
}  // namespace render
}  // namespace kryga

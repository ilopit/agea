#pragma once

#include <atomic>
#include <bit>
#include <cstdint>
#include <deque>
#include <memory>
#include <thread>

#include <utils/check.h>

namespace kryga
{
namespace render
{
namespace types
{

namespace detail
{
// ---------------------------------------------------------------------------
// thread_affinity
//
// Debug-only guard that asserts a primitive is always touched by the SAME thread
// it was bound to. The handle model splits work across a model thread and a
// render thread by *contract* (no lock enforces it any more); this turns that
// contract into a runtime check in development and compiles to nothing in ship.
//
// First touch lazily binds (so single-threaded use needs no setup). bind() /
// reset() handle the lifecycle handoff: a pool is created + preallocated on the
// init thread, then ownership transfers to the model/render thread, which calls
// bind() to re-stamp the owner.
// ---------------------------------------------------------------------------
class thread_affinity
{
public:
    // (Re)assign the owner to the calling thread.
    void
    bind()
    {
#ifndef NDEBUG
        m_owner = std::this_thread::get_id();
        m_bound = true;
#endif
    }

    // Drop the binding; the next check()/bind() re-stamps. Used at teardown.
    void
    reset()
    {
#ifndef NDEBUG
        m_bound = false;
#endif
    }

    // Assert the caller is the owner. Lazily binds on first touch. Calls assert_fail
    // directly (not KRG_check) because the per-site message is a runtime pointer,
    // while KRG_check's macro needs a string literal to stringize.
    void
    check(const char* what) const
    {
#ifndef NDEBUG
        auto self = std::this_thread::get_id();
        if (!m_bound)
        {
            m_owner = self;
            m_bound = true;
            return;
        }
        if (m_owner != self)
        {
            ::kryga::utils::assert_fail(what, __FILE__, __LINE__);
        }
#else
        (void)what;
#endif
    }

#ifndef NDEBUG
private:
    mutable std::thread::id m_owner{};
    mutable bool m_bound = false;
#endif
};
}  // namespace detail

// ---------------------------------------------------------------------------
// handle<Kind>
//
// Typed U64 render-resource handle. Bit layout: 24 index | 32 generation | 8 kind.
//   - index      pool slot (24 bits -> 16M slots; deliberately overkill)
//   - generation dangling-detection counter (32 bits -> effectively never wraps)
//   - kind       compile-time resource type tag (8 bits)
//
// v == 0 is the null handle. A live slot's generation is never 0, so a live
// handle is never all-zero even at index 0 / kind 0.
//
// Distinct Kind values are distinct C++ types: passing a mesh handle where a
// texture handle is expected is a compile error, not a runtime tag mismatch.
// ---------------------------------------------------------------------------
template <uint8_t Kind>
struct handle
{
    static constexpr uint64_t index_bits = 24;
    static constexpr uint64_t gen_bits = 32;
    static constexpr uint64_t index_mask = (uint64_t(1) << index_bits) - 1;  // 0xFF'FFFF
    static constexpr uint64_t gen_mask = (uint64_t(1) << gen_bits) - 1;      // 0xFFFF'FFFF

    uint64_t v = 0;

    static handle
    make(uint32_t index, uint32_t generation)
    {
        handle h;
        h.v = (uint64_t(Kind) << (index_bits + gen_bits)) |
              ((uint64_t(generation) & gen_mask) << index_bits) | (uint64_t(index) & index_mask);
        return h;
    }

    uint32_t
    index() const
    {
        return uint32_t(v & index_mask);
    }

    uint32_t
    generation() const
    {
        return uint32_t((v >> index_bits) & gen_mask);
    }

    uint8_t
    kind() const
    {
        return uint8_t(v >> (index_bits + gen_bits));
    }

    explicit
    operator bool() const
    {
        return v != 0;
    }
    bool
    operator==(const handle& o) const
    {
        return v == o.v;
    }
    bool
    operator!=(const handle& o) const
    {
        return v != o.v;
    }
};

// ---------------------------------------------------------------------------
// slot_state
//
// Per-slot residency. The render thread owns transitions; the tri-state in the
// middle separates a legitimate async window (pending -> draw placeholder) from
// a contract violation (reserved -> assert: a draw was issued before populate).
// ---------------------------------------------------------------------------
enum class slot_state : uint8_t
{
    free = 0,  // on the free-list, not handed out
    reserved,  // handle handed out, no populate submitted -- drawing it is a bug
    pending,   // populate submitted, GPU upload in flight -- draw placeholder
    resident,  // upload complete, real data available
    retiring,  // freed, awaiting safe reclaim (GPU still draining in-flight frames)
};

// ---------------------------------------------------------------------------
// i_slot_grower
//
// The ONE thing handle_allocator needs from slot_storage: "make index N
// addressable". Pulling it behind a non-templated interface keeps the allocator
// free of the storage's element type T -- so a model-layer holder (render_translator,
// packages) can name and operate handle_allocator<Kind> without dragging in the
// render-side T (mesh_data, material_data, ...). grow_to is rare (once per chunk),
// so the virtual call is irrelevant; the hot reads (at/valid) stay non-virtual.
// ---------------------------------------------------------------------------
class i_slot_grower
{
public:
    virtual ~i_slot_grower() = default;
    virtual void
    grow_to(uint32_t count) = 0;
};

// ---------------------------------------------------------------------------
// slot_storage<Kind, T>
//
// The data array, indexed by handle index. Holds the actual T (mesh_data,
// material_data, ...) interleaved with a generation SHADOW in one `slot`.
//
// LOCK-FREE growable, single-grower / single-reader. The backing is a fixed
// table of pointer-stable CHUNKS: growth appends a chunk (published release) and
// bumps an atomic size -- it NEVER moves an existing chunk, so a live `T*` stays
// valid forever and a reader indexes with one acquire load, no lock. This is the
// whole point of the chunked layout: the old std::deque needed a mutex only
// because emplace_back could reallocate its block map under a concurrent reader;
// a chunk table that only ever grows at the end removes that race structurally.
//
// Threading contract:
//   - grow_to()  -> MODEL thread, the SOLE grower. Installs chunks + advances
//                   size. (handle_allocator drives this from reserve/preallocate.)
//   - everything else -> RENDER thread, the SOLE reader/populater. Reads chunk
//                   pointers + size with acquire; writes slot contents (data, gen)
//                   which no other thread touches.
//   - clear()    -> teardown only, NOT concurrent with readers (level unload).
//
// Publish order is the correctness hinge: grow_to stores the chunk pointer
// (release) BEFORE the size (release); a reader loads size (acquire) BEFORE the
// chunk (acquire). Observing a size that covers index i therefore guarantees the
// chunk backing i -- and its zero-initialized contents -- are visible.
//
// The shadow generation: the model-owned allocator holds the authoritative
// generation; the render thread mirrors it here at populate (set_generation), so
// it can answer valid()/occupied() without ever touching the model's allocator.
// ---------------------------------------------------------------------------
template <uint8_t Kind, typename T>
class slot_storage : public i_slot_grower
{
public:
    using handle_t = handle<Kind>;

    // Defaults: 1024 slots/chunk, ~1M total -> 1024-pointer table (8 KB). Override
    // per pool: a mesh pool can be tiny (few meshes), a material pool large. Memory
    // is `max_slots/chunk_size` pointers up front, then chunk_size*sizeof(slot) per
    // chunk on demand. Smaller chunk_size = finer growth granularity + bigger table;
    // larger chunk_size = coarser growth + smaller table.
    static constexpr uint32_t k_default_chunk_size = 1024;
    static constexpr uint32_t k_default_max_slots = uint32_t(1) << 20;  // ~1M

    // chunk_size MUST be a power of two (the index split is shift + mask). max_slots
    // is rounded up to a whole number of chunks. Both are capped by the 24-bit
    // handle index space.
    explicit slot_storage(uint32_t chunk_size = k_default_chunk_size,
                          uint32_t max_slots = k_default_max_slots)
    {
        KRG_check(chunk_size > 0 && (chunk_size & (chunk_size - 1)) == 0,
                  "chunk_size must be a power of two");
        KRG_check(uint64_t(max_slots) <= uint64_t(handle_t::index_mask) + 1,
                  "max_slots exceeds the 24-bit handle index space");

        m_chunk_size = chunk_size;
        m_chunk_mask = chunk_size - 1;
        m_chunk_bits = uint32_t(std::countr_zero(chunk_size));        // log2(chunk_size)
        m_max_chunks = (max_slots + chunk_size - 1) >> m_chunk_bits;  // ceil(max/chunk)
        m_max_slots = m_max_chunks * chunk_size;                      // rounded up
        m_chunks = std::make_unique<std::atomic<slot*>[]>(m_max_chunks);
    }

    ~slot_storage()
    {
        free_chunks();
    }

    slot_storage(const slot_storage&) = delete;
    slot_storage&
    operator=(const slot_storage&) = delete;

    // Re-stamp thread ownership at the init->steady-state handoff. Call grower bind
    // from the model thread, reader bind from the render thread.
    void
    bind_grower_to_current_thread()
    {
        m_grower.bind();
    }
    void
    bind_reader_to_current_thread()
    {
        m_reader.bind();
    }

    // [model thread] Grow so that indices 0..count-1 are addressable. Driven by
    // handle_allocator when it reserves an index past current capacity. Fresh slots
    // are value-initialized: data == T{}, shadow generation == 0. Gen 0 matches no
    // live handle (allocator generations start at 1), so an un-populated slot reads
    // as invalid. The SOLE grower -- never called concurrently with itself.
    void
    grow_to(uint32_t count) override
    {
        m_grower.check("slot_storage::grow_to from a non-grower thread");
        KRG_check(count <= m_max_slots, "slot pool capacity exhausted (raise max_slots)");
        uint32_t cur = m_size.load(std::memory_order_relaxed);  // only this thread writes m_size
        while (cur < count)
        {
            uint32_t ci = cur >> m_chunk_bits;
            if (m_chunks[ci].load(std::memory_order_acquire) == nullptr)
            {
                m_chunks[ci].store(new slot[m_chunk_size](), std::memory_order_release);
            }
            uint32_t chunk_end = (ci + 1) << m_chunk_bits;
            cur = count < chunk_end ? count : chunk_end;
            m_size.store(cur, std::memory_order_release);  // publish AFTER the chunk
        }
    }

    // [render thread] Record the generation now occupying this slot (called on
    // populate with the handle's generation, on retire/reset with 0). Maintains
    // the live counter on empty<->occupied transitions.
    void
    set_generation(uint32_t index, uint32_t generation)
    {
        m_reader.check("slot_storage::set_generation from a non-reader thread");
        auto& gen = slot_at(index)->gen;
        if ((gen == 0) && (generation != 0))
        {
            ++m_active;
        }
        else if ((gen != 0) && (generation == 0))
        {
            --m_active;
        }
        gen = generation;
    }

    // [render thread] Deref. The returned pointer is stable forever (the chunk is
    // never moved or freed until clear()), so callers may use it after this returns.
    T*
    at(uint32_t index)
    {
        m_reader.check("slot_storage::at from a non-reader thread");
        KRG_check_debug(index < m_size.load(std::memory_order_acquire),
                        "deref of an index past the populated range");
        return &slot_at(index)->data;
    }

    // [render thread] Whether a slot currently holds a live (populated, non-retired)
    // element. Retired/empty slots carry shadow generation 0 -- no live handle has
    // gen 0, so this also answers "is this index occupied" without a handle in hand.
    bool
    occupied(uint32_t index) const
    {
        m_reader.check("slot_storage::occupied from a non-reader thread");
        return index < m_size.load(std::memory_order_acquire) && slot_at(index)->gen != 0;
    }

    // [render thread] Functional liveness: index addressable AND shadow generation
    // matches. Used to skip work for a slot freed out from under an in-flight command.
    bool
    valid(handle_t h) const
    {
        m_reader.check("slot_storage::valid from a non-reader thread");
        if (!h)
        {
            return false;
        }
        uint32_t idx = h.index();
        if (idx >= m_size.load(std::memory_order_acquire))
        {
            return false;
        }
        return slot_at(idx)->gen == h.generation();
    }

    // [render thread] Free the data and invalidate the slot. Shadow generation
    // goes to 0 ("empty"): no live handle has gen 0, so outstanding handles fail
    // valid() AND occupied() correctly reports the slot free (the previous
    // bump-the-shadow variant left occupied() true for reset slots).
    void
    reset(uint32_t index)
    {
        m_reader.check("slot_storage::reset from a non-reader thread");
        slot_at(index)->data = T{};
        set_generation(index, 0);
    }

    // Re-stamp BOTH thread bindings to the calling thread at a lifecycle
    // handoff. In the laned design every storage touch (grow_for, at, reset at
    // populate) runs on the one consumer thread: init builds on the main
    // thread, then the render thread takes over, then main reclaims for
    // teardown. Mirrors handle_allocator::bind_to_current_thread().
    void
    bind_to_current_thread()
    {
        m_grower.bind();
        m_reader.bind();
    }

    // [teardown, single-threaded] Free every chunk and reset to empty. Must NOT run
    // concurrently with readers -- a render-thread at()/valid() racing a free is UB.
    // Drops both thread bindings: a fresh lifecycle re-stamps them.
    void
    clear()
    {
        free_chunks();
        m_size.store(0, std::memory_order_relaxed);
        m_active = 0;
        m_grower.reset();
        m_reader.reset();
    }

    uint64_t
    size() const
    {
        return m_size.load(std::memory_order_acquire);
    }

    // Live (populated, non-retired) slot count. Render-thread bookkeeping —
    // introspection / published stats.
    uint64_t
    active() const
    {
        return m_active;
    }

    // Alias for size(): the number of addressable slots.
    uint64_t
    capacity() const
    {
        return size();
    }

private:
    struct slot
    {
        T data{};
        uint32_t gen = 0;  // shadow generation; 0 == empty / never populated
    };

    slot*
    slot_at(uint32_t index) const
    {
        return m_chunks[index >> m_chunk_bits].load(std::memory_order_acquire) +
               (index & m_chunk_mask);
    }

    void
    free_chunks()
    {
        uint32_t n = m_size.load(std::memory_order_relaxed);
        uint32_t chunks = (n + m_chunk_mask) >> m_chunk_bits;  // ceil(n / chunk_size)
        for (uint32_t i = 0; i < chunks; ++i)
        {
            delete[] m_chunks[i].load(std::memory_order_relaxed);
            m_chunks[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    // Per-pool sizing, fixed at construction (see ctor).
    uint32_t m_chunk_size = 0;
    uint32_t m_chunk_bits = 0;  // log2(m_chunk_size); index >> bits -> chunk
    uint32_t m_chunk_mask = 0;  // m_chunk_size - 1; index & mask -> offset
    uint32_t m_max_chunks = 0;  // table length
    uint32_t m_max_slots = 0;   // m_max_chunks * m_chunk_size

    // Fixed table of pointer-stable chunks. The table base never moves (set once in
    // the ctor); chunks are appended at the end and never relocated.
    std::unique_ptr<std::atomic<slot*>[]> m_chunks;
    std::atomic<uint32_t> m_size{0};
    // Live slot count; written only by the reader (render) thread alongside the
    // generation shadow, so a plain integer is enough.
    uint64_t m_active = 0;

    detail::thread_affinity m_grower;  // model thread: grow_to
    detail::thread_affinity m_reader;  // render thread: at/valid/occupied/set_generation/reset
};

// ---------------------------------------------------------------------------
// handle_allocator<Kind>
//
// Slot identity management: free-list + generation table + per-slot state. Owns
// NO data array and is NOT parameterized on the storage element type T -- it grows
// its bound storage through the non-templated i_slot_grower interface, so a
// model-layer holder can use it without knowing the render-side T. Bind it to a
// slot_storage once (ctor or bind_storage) and it asks that storage for more slots
// (grow_to) whenever it hands out an index past current capacity. A null grower is
// allowed (pure-allocator use): reserve() then only grows its own bookkeeping.
//
// This is the MODEL-thread half of the split: every method runs on the single
// owning (model) thread -- the thread_affinity guard asserts that in debug. The
// only cross-thread touch is the storage growth, made safe by the storage's own
// lock-free chunk publication (release/acquire), not a lock.
//
// Authoritative generation lives here (m_entries); the storage holds a shadow it
// mirrors at populate time, so each thread can answer valid() against its own
// copy without a cross-thread read.
// ---------------------------------------------------------------------------
template <uint8_t Kind>
class handle_allocator
{
public:
    using handle_t = handle<Kind>;

    handle_allocator() = default;

    explicit handle_allocator(i_slot_grower& storage)
        : m_grower(&storage)
    {
    }

    // Bind (or rebind) the storage this allocator grows. Call at init, before the
    // first reserve, on the model thread.
    void
    bind_storage(i_slot_grower& storage)
    {
        m_grower = &storage;
    }

    // Re-stamp ownership at the init->model-thread handoff (the allocator is the
    // model-thread half, so this is the model thread).
    void
    bind_to_current_thread()
    {
        m_affinity.bind();
    }

    // Pre-grow to n free slots in one shot (init-time bulk reservation). After
    // this, reserve() hands out ascending indices (0, 1, 2, ...) without any
    // per-call growth. Call once on a fresh allocator (single-threaded init); it
    // clears any existing free-list and sizes the bound storage to match.
    void
    preallocate(uint32_t n)
    {
        m_affinity.check("handle_allocator::preallocate from a non-owner thread");
        while (m_entries.size() < n)
        {
            m_entries.emplace_back();
        }
        m_free.clear();
        // Push descending so the back (which reserve() pops first) is index 0.
        for (uint32_t i = n; i-- > 0;)
        {
            m_free.push_back(i);
        }
        if (m_grower)
        {
            m_grower->grow_to(n);
        }
    }

    // Reserve one empty-but-valid slot, growing the storage if needed.
    handle_t
    reserve()
    {
        m_affinity.check("handle_allocator::reserve from a non-owner thread");
        uint32_t idx;
        if (!m_free.empty())
        {
            idx = m_free.back();
            m_free.pop_back();
        }
        else
        {
            idx = uint32_t(m_entries.size());
            KRG_check(uint64_t(idx) <= handle_t::index_mask, "24-bit slot index space exhausted");
            m_entries.emplace_back();
        }

        // Always ensure the bound storage covers idx. Cheap no-op when already
        // sized; the point is the free-list path -- after the storage is cleared
        // (level reload) a recycled index would otherwise have no backing slot,
        // and populate no longer grows (growth is model-thread-only now).
        if (m_grower)
        {
            m_grower->grow_to(idx + 1);
        }

        auto& e = m_entries[idx];
        e.state = slot_state::reserved;
        ++m_active;
        return handle_t::make(idx, e.generation);
    }

    // Reclaim a slot immediately: bump generation (every outstanding handle to it
    // becomes stale) and return the index to the free-list right away. For
    // single-threaded / synchronous use (the system pool).
    void
    reclaim(handle_t h)
    {
        m_affinity.check("handle_allocator::reclaim from a non-owner thread");
        bump_generation(h);
        m_entries[h.index()].state = slot_state::free;
        --m_active;
        m_free.push_back(h.index());
    }

    // Deferred free (the model-owned content allocator): bump generation now (so
    // the handle is immediately invalid for future reservations) but DON'T return
    // the index to the free-list yet -- park it, tagged with the current tick. A
    // later tick() returns it to the free-list once `defer_ticks` have passed,
    // which guarantees the GPU has drained any in-flight frame that referenced
    // the slot. No cross-thread structure: the whole allocator is model-owned.
    void
    free(handle_t h)
    {
        m_affinity.check("handle_allocator::free from a non-owner thread");
        // Null is "nothing was reserved" (a component with no render object) --
        // a legitimate no-op. A stale (non-null, wrong-gen) handle is a double-free
        // bug and still asserts via bump_generation -> valid().
        if (!h)
        {
            return;
        }
        bump_generation(h);
        m_entries[h.index()].state = slot_state::retiring;
        --m_active;
        m_retiring.push_back({h.index(), m_tick});
    }

    // Advance one frame and mature retired slots whose deferral window elapsed.
    // Call once per frame on the owning (model) thread. Retiring is FIFO by tick.
    void
    tick()
    {
        m_affinity.check("handle_allocator::tick from a non-owner thread");
        ++m_tick;
        while (!m_retiring.empty() && m_retiring.front().retire_tick + m_defer_ticks <= m_tick)
        {
            uint32_t idx = m_retiring.front().index;
            m_retiring.pop_front();
            m_entries[idx].state = slot_state::free;
            m_free.push_back(idx);
        }
    }

    uint64_t
    retiring_count() const
    {
        m_affinity.check("handle_allocator::retiring_count from a non-owner thread");
        return m_retiring.size();
    }

    // Set the deferral window: how many ticks a freed slot stays parked before it
    // can be reserved again. Drive this from frames_in_flight so it matches the
    // GPU-resource delete queue's horizon (one source of truth for "GPU drained").
    void
    set_defer_ticks(uint64_t n)
    {
        m_affinity.check("handle_allocator::set_defer_ticks from a non-owner thread");
        m_defer_ticks = n;
    }

    // Generation + liveness check.
    bool
    valid(handle_t h) const
    {
        m_affinity.check("handle_allocator::valid from a non-owner thread");
        if (!h)
        {
            return false;
        }
        uint32_t idx = h.index();
        if (idx >= m_entries.size())
        {
            return false;
        }
        return m_entries[idx].generation == h.generation() &&
               m_entries[idx].state != slot_state::free;
    }

    slot_state
    state(handle_t h) const
    {
        KRG_check(valid(h), "state query on a stale or null handle");
        return m_entries[h.index()].state;
    }

    void
    set_state(handle_t h, slot_state st)
    {
        KRG_check(valid(h), "set_state on a stale or null handle");
        m_entries[h.index()].state = st;
    }

    void
    clear()
    {
        m_entries.clear();
        m_free.clear();
        m_retiring.clear();
        m_active = 0;
        m_tick = 0;
        m_affinity.reset();
    }

    uint64_t
    active_count() const
    {
        m_affinity.check("handle_allocator::active_count from a non-owner thread");
        return m_active;
    }

    uint64_t
    capacity() const
    {
        m_affinity.check("handle_allocator::capacity from a non-owner thread");
        return m_entries.size();
    }

private:
    struct entry
    {
        uint32_t generation = 1;  // starts at 1 -- generation 0 == null handle
        slot_state state = slot_state::free;
    };

    struct retire_record
    {
        uint32_t index;
        uint64_t retire_tick;
    };

    void
    bump_generation(handle_t h)
    {
        KRG_check(valid(h), "free/reclaim of a stale or already-free handle");
        auto& e = m_entries[h.index()];
        ++e.generation;
        if (e.generation == 0)
        {
            e.generation = 1;  // never 0 -- that's the null sentinel
        }
    }

    i_slot_grower* m_grower = nullptr;     // the storage this allocator grows (nullable)
    std::deque<entry> m_entries;           // generation + state per index
    std::deque<uint32_t> m_free;           // recycled indices, ready to reserve
    std::deque<retire_record> m_retiring;  // freed, awaiting deferral maturation
    uint64_t m_active = 0;
    uint64_t m_tick = 0;
    // Deferral window in ticks; set from frames_in_flight + slack. Default is a
    // safe small value until configured (covers a 1-deep pipeline).
    uint64_t m_defer_ticks = 3;

    detail::thread_affinity m_affinity;  // the single owning (model) thread
};

}  // namespace types
}  // namespace render
}  // namespace kryga

#pragma once

#include <cstdint>

namespace kryga
{
namespace utils
{

// ---------------------------------------------------------------------------
// handle<Kind>
//
// Typed U64 resource handle. Bit layout: 24 index | 32 generation | 8 kind.
//   - index      pool slot (24 bits -> 16M slots; deliberately overkill)
//   - generation dangling-detection counter (32 bits -> effectively never wraps)
//   - kind       compile-time resource type tag (8 bits)
//
// v == 0 is the null handle. A live slot's generation is never 0, so a live
// handle is never all-zero even at index 0 / kind 0.
//
// Distinct Kind values are distinct C++ types: passing a mesh handle where a
// texture handle is expected is a compile error, not a runtime tag mismatch.
//
// This is a pure value type -- a held identity, no pool machinery. The chunked
// storage + allocator that mint and resolve handles live in utils/laned_pool.h;
// code that only HOLDS a handle (model components, asset records) includes just
// this header and never pays for the pool's atomics/chunk implementation.
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
// Per-slot residency. The consumer thread owns transitions; the tri-state in the
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

}  // namespace utils
}  // namespace kryga

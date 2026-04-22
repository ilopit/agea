#pragma once

#include <cstdint>
#include <functional>

namespace kryga
{
namespace utils
{

// Packed 64-bit handle into a render-side slot-allocated cache.
// Layout: [ 16 bits generation | 48 bits slot index ]
// Tag is a phantom type for type safety — handles for different caches
// are not convertible even though the underlying value type is the same.
template <typename Tag>
struct slot_handle
{
    using tag_type = Tag;

    static constexpr uint64_t SLOT_BITS = 48;
    static constexpr uint64_t GEN_BITS = 16;
    static constexpr uint64_t SLOT_MASK = (uint64_t{1} << SLOT_BITS) - 1;
    static constexpr uint64_t GEN_MASK = (uint64_t{1} << GEN_BITS) - 1;
    static constexpr uint32_t INVALID_SLOT = static_cast<uint32_t>(SLOT_MASK);

    uint64_t value = pack(INVALID_SLOT, 0);

    slot_handle() = default;

    explicit slot_handle(uint64_t raw)
        : value(raw)
    {
    }

    slot_handle(uint32_t slot, uint16_t gen)
        : value(pack(slot, gen))
    {
    }

    static constexpr uint64_t
    pack(uint32_t slot, uint16_t gen)
    {
        return (uint64_t{gen} << SLOT_BITS) | (uint64_t{slot} & SLOT_MASK);
    }

    uint32_t
    slot() const
    {
        return static_cast<uint32_t>(value & SLOT_MASK);
    }

    uint16_t
    gen() const
    {
        return static_cast<uint16_t>((value >> SLOT_BITS) & GEN_MASK);
    }

    bool
    is_valid() const
    {
        return slot() != INVALID_SLOT;
    }

    static slot_handle
    invalid()
    {
        return slot_handle{INVALID_SLOT, 0};
    }

    bool
    operator==(const slot_handle& other) const
    {
        return value == other.value;
    }

    bool
    operator!=(const slot_handle& other) const
    {
        return value != other.value;
    }
};

}  // namespace utils
}  // namespace kryga

namespace std
{
template <typename Tag>
struct hash<kryga::utils::slot_handle<Tag>>
{
    size_t
    operator()(const kryga::utils::slot_handle<Tag>& h) const noexcept
    {
        return std::hash<uint64_t>{}(h.value);
    }
};
}  // namespace std

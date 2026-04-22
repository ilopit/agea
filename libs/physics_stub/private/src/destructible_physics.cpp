#include "physics_stub/destructible_physics.h"

#include <unordered_map>

namespace kryga
{
namespace physics_stub
{

struct entry
{
    std::vector<chunk_shape> chunks;
    float damage_threshold = 0.0f;
    float accumulated_damage = 0.0f;
    bool broken = false;
};

struct destructible_physics::impl
{
    std::unordered_map<uint64_t, entry> entries;
    uint64_t next_id = 1;
};

destructible_physics::destructible_physics()
    : m_impl(new impl)
{
}

destructible_physics::~destructible_physics()
{
    delete m_impl;
}

destructible_handle
destructible_physics::register_destructible(const std::vector<chunk_shape>& chunks,
                                            float damage_threshold)
{
    destructible_handle h;
    h.value = m_impl->next_id++;

    entry e;
    e.chunks = chunks;
    e.damage_threshold = damage_threshold;
    m_impl->entries.emplace(h.value, std::move(e));

    return h;
}

void
destructible_physics::unregister_destructible(destructible_handle h)
{
    if (!h.valid())
    {
        return;
    }
    m_impl->entries.erase(h.value);
}

bool
destructible_physics::apply_impact(destructible_handle h, const impact& hit)
{
    if (!h.valid())
    {
        return false;
    }
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return false;
    }

    auto& e = it->second;
    if (e.broken)
    {
        return false;
    }

    e.accumulated_damage += hit.damage;
    if (e.accumulated_damage >= e.damage_threshold)
    {
        e.broken = true;
        return true;
    }
    return false;
}

bool
destructible_physics::is_broken(destructible_handle h) const
{
    if (!h.valid())
    {
        return false;
    }
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return false;
    }
    return it->second.broken;
}

glm::mat4
destructible_physics::get_chunk_transform(destructible_handle h, uint32_t) const
{
    (void)h;
    return glm::mat4(1.0f);
}

}  // namespace physics_stub
}  // namespace kryga

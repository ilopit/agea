#include <physics/destructible_physics.h>
#include <physics/physics_system.h>

#include "physics_internal/physics_system_impl.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm_unofficial/glm.h>

#include <random>
#include <unordered_map>

namespace kryga
{
namespace physics
{

namespace
{

JPH::Vec3
to_jph(const glm::vec3& v)
{
    return JPH::Vec3(v.x, v.y, v.z);
}

glm::mat4
compose_transform(const JPH::RVec3& pos, const JPH::Quat& rot)
{
    glm::quat q(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
    glm::mat4 r = glm::mat4_cast(q);
    r[3] = glm::vec4(pos.GetX(), pos.GetY(), pos.GetZ(), 1.0f);
    return r;
}

}  // namespace

struct entry
{
    // Shape data (populated on register, read on break).
    std::vector<chunk_shape> chunks;
    float damage_threshold = 100.0f;
    float accumulated_damage = 0.0f;

    // Component-provided state.
    glm::mat4 world_transform{1.0f};
    float lifetime = 3.0f;
    float explosion_strength = 8.0f;

    // Runtime state.
    bool broken = false;
    float age_since_break = 0.0f;  // seconds
    std::vector<JPH::BodyID> chunk_bodies;  // empty until break
};

struct destructible_physics::impl
{
    physics_system* owner = nullptr;
    std::unordered_map<uint64_t, entry> entries;
    uint64_t next_id = 1;
};

destructible_physics::destructible_physics(physics_system& owner)
    : m_impl(std::make_unique<impl>())
{
    m_impl->owner = &owner;
}

destructible_physics::~destructible_physics()
{
    // Best-effort cleanup of live bodies.
    if (m_impl && m_impl->owner && m_impl->owner->m_impl && m_impl->owner->m_impl->world)
    {
        auto& bi = m_impl->owner->m_impl->world->GetBodyInterface();
        for (auto& kv : m_impl->entries)
        {
            for (auto& bid : kv.second.chunk_bodies)
            {
                if (!bid.IsInvalid())
                {
                    bi.RemoveBody(bid);
                    bi.DestroyBody(bid);
                }
            }
        }
    }
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
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return;
    }

    // Tear down any bodies we spawned.
    if (m_impl->owner->m_impl->world)
    {
        auto& bi = m_impl->owner->m_impl->world->GetBodyInterface();
        for (auto& bid : it->second.chunk_bodies)
        {
            if (!bid.IsInvalid())
            {
                bi.RemoveBody(bid);
                bi.DestroyBody(bid);
            }
        }
    }
    m_impl->entries.erase(it);
}

void
destructible_physics::set_world_transform(destructible_handle h, const glm::mat4& world_mat)
{
    if (!h.valid())
    {
        return;
    }
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return;
    }
    // Updating world_transform after break is a no-op — chunks fly on their own.
    if (!it->second.broken)
    {
        it->second.world_transform = world_mat;
    }
}

void
destructible_physics::set_lifetime(destructible_handle h, float seconds)
{
    if (!h.valid())
    {
        return;
    }
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return;
    }
    it->second.lifetime = seconds;
}

void
destructible_physics::set_explosion_strength(destructible_handle h, float strength)
{
    if (!h.valid())
    {
        return;
    }
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return;
    }
    it->second.explosion_strength = strength;
}

// Core break path — creates Jolt bodies for each chunk. Called from
// apply_impact() and shatter().
static void
spawn_chunk_bodies(entry& e, JPH::PhysicsSystem& world, const glm::vec3& impulse_origin)
{
    if (e.broken)
    {
        return;
    }
    e.broken = true;
    e.age_since_break = 0.0f;

    auto& bi = world.GetBodyInterface();

    // Deterministic per-entry RNG for angular variation.
    std::mt19937 rng(static_cast<uint32_t>(e.chunks.size()) * 0x9E3779B1u);
    std::uniform_real_distribution<float> ud(-1.0f, 1.0f);

    e.chunk_bodies.reserve(e.chunks.size());

    for (const auto& ck : e.chunks)
    {
        // Chunk seed point is in mesh-local space. Map through the
        // destructible's current world transform to get world placement.
        glm::vec4 seed_world4 = e.world_transform * glm::vec4(ck.seed_point, 1.0f);
        glm::vec3 seed_world(seed_world4);

        glm::vec3 half_extent_local = 0.5f * (ck.aabb_max - ck.aabb_min);
        // Guard against degenerate AABBs (single-vertex chunks, etc.).
        half_extent_local = glm::max(half_extent_local, glm::vec3(0.01f));

        // Approximate scale pulled from the world transform.
        glm::vec3 world_scale(glm::length(glm::vec3(e.world_transform[0])),
                              glm::length(glm::vec3(e.world_transform[1])),
                              glm::length(glm::vec3(e.world_transform[2])));
        glm::vec3 half_extent_world = half_extent_local * world_scale;
        half_extent_world = glm::max(half_extent_world, glm::vec3(0.01f));

        JPH::BoxShapeSettings box_settings(to_jph(half_extent_world));
        box_settings.SetEmbedded();
        auto sr = box_settings.Create();
        if (sr.HasError())
        {
            e.chunk_bodies.push_back({});
            continue;
        }

        JPH::BodyCreationSettings bcs(sr.Get(),
                                      JPH::RVec3(seed_world.x, seed_world.y, seed_world.z),
                                      JPH::Quat::sIdentity(),
                                      JPH::EMotionType::Dynamic,
                                      jolt_layers::MOVING);

        // Linear/angular damping so chunks settle rather than skid forever.
        bcs.mLinearDamping = 0.1f;
        bcs.mAngularDamping = 0.2f;
        // Prevent tunneling of small chunks at high impulse.
        bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;

        JPH::BodyID bid = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);

        // Outward impulse: direction from impact origin to chunk, scaled by
        // explosion strength. Add small random angular impulse for variety.
        glm::vec3 outward = seed_world - impulse_origin;
        float len2 = glm::dot(outward, outward);
        glm::vec3 impulse_dir;
        if (len2 > 1e-6f)
        {
            impulse_dir = outward / std::sqrt(len2);
        }
        else
        {
            impulse_dir = glm::vec3(ud(rng), std::abs(ud(rng)) + 0.2f, ud(rng));
            impulse_dir = glm::normalize(impulse_dir);
        }

        float mag = e.explosion_strength;
        glm::vec3 impulse = impulse_dir * mag + glm::vec3(0.0f, 0.5f * mag, 0.0f);
        bi.AddImpulse(bid, to_jph(impulse));
        bi.AddAngularImpulse(
            bid, JPH::Vec3(ud(rng) * mag * 0.2f, ud(rng) * mag * 0.2f, ud(rng) * mag * 0.2f));

        e.chunk_bodies.push_back(bid);
    }
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
    if (e.accumulated_damage < e.damage_threshold)
    {
        return false;
    }

    if (!m_impl->owner->m_impl->world)
    {
        return false;
    }

    // Use the hit point as impulse origin if provided; else fall back to the
    // destructible's current world origin.
    glm::vec3 origin(e.world_transform[3]);
    if (glm::dot(hit.point, hit.point) > 1e-6f)
    {
        origin = hit.point;
    }

    spawn_chunk_bodies(e, *m_impl->owner->m_impl->world, origin);
    return true;
}

bool
destructible_physics::shatter(destructible_handle h)
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

    if (!m_impl->owner->m_impl->world)
    {
        return false;
    }

    glm::vec3 origin(e.world_transform[3]);
    spawn_chunk_bodies(e, *m_impl->owner->m_impl->world, origin);
    return true;
}

bool
destructible_physics::is_broken(destructible_handle h) const
{
    if (!h.valid())
    {
        return false;
    }
    auto it = m_impl->entries.find(h.value);
    return it != m_impl->entries.end() && it->second.broken;
}

bool
destructible_physics::is_expired(destructible_handle h) const
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
    const auto& e = it->second;
    return e.broken && e.age_since_break >= e.lifetime;
}

glm::mat4
destructible_physics::get_chunk_transform(destructible_handle h, uint32_t chunk_index) const
{
    if (!h.valid())
    {
        return glm::mat4(1.0f);
    }
    auto it = m_impl->entries.find(h.value);
    if (it == m_impl->entries.end())
    {
        return glm::mat4(1.0f);
    }
    const auto& e = it->second;
    if (!e.broken || chunk_index >= e.chunk_bodies.size())
    {
        return e.world_transform;
    }

    auto bid = e.chunk_bodies[chunk_index];
    if (bid.IsInvalid() || !m_impl->owner->m_impl->world)
    {
        return e.world_transform;
    }

    auto& bi = m_impl->owner->m_impl->world->GetBodyInterface();
    auto pos = bi.GetPosition(bid);
    auto rot = bi.GetRotation(bid);
    return compose_transform(pos, rot);
}

void
destructible_physics::tick(float dt)
{
    for (auto& kv : m_impl->entries)
    {
        auto& e = kv.second;
        if (e.broken)
        {
            e.age_since_break += dt;
        }
    }
}

}  // namespace physics
}  // namespace kryga

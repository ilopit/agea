#include <physics/physics_system.h>
#include <physics/destructible_physics.h>

#include <global_state/global_state.h>

#include <utils/kryga_log.h>

#include "physics_internal/physics_system_impl.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace kryga
{

void
state_mutator__physics_system::set(gs::state& s)
{
    auto p = s.create_box<physics::physics_system>("physics_system");
    s.m_physics_system = p;
}

namespace physics
{

physics_system::physics_system()
    : m_impl(std::make_unique<impl>())
{
}

physics_system::~physics_system()
{
    if (m_impl && m_impl->world)
    {
        shutdown();
    }
}

void
physics_system::init()
{
    JPH::RegisterDefaultAllocator();

    if (JPH::Factory::sInstance == nullptr)
    {
        JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();

    // 10 MiB scratch buffer for per-step work.
    m_impl->temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    m_impl->job_system = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

    m_impl->world = std::make_unique<JPH::PhysicsSystem>();
    m_impl->world->Init(
        /*inMaxBodies=*/2048,
        /*inNumBodyMutexes=*/0,
        /*inMaxBodyPairs=*/2048,
        /*inMaxContactConstraints=*/1024,
        m_impl->bp_layers,
        m_impl->obj_vs_bp,
        m_impl->obj_vs_obj);

    m_impl->world->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    m_destructibles = std::make_unique<destructible_physics>(*this);
}

void
physics_system::shutdown()
{
    // Destructibles reference bodies in the world; free them first.
    m_destructibles.reset();

    if (m_impl->world && !m_impl->static_world_body.IsInvalid())
    {
        auto& bi = m_impl->world->GetBodyInterface();
        bi.RemoveBody(m_impl->static_world_body);
        bi.DestroyBody(m_impl->static_world_body);
        m_impl->static_world_body = {};
    }

    if (m_impl->world)
    {
        // The physics thread is joined by now (the engine stops threads before
        // shutdown), so re-stamp the storage's lane affinity to this (main) thread
        // before the sweep — the sanctioned teardown handoff. Single lane, so iterate
        // it directly in local index space. Destroy any live body in an occupied slot.
        m_impl->static_storage.bind_to_current_thread();
        auto& bi = m_impl->world->GetBodyInterface();
        auto& lane = m_impl->static_storage.lane(0);
        const uint64_t n = lane.size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (!lane.occupied(i))
            {
                continue;
            }
            const JPH::BodyID body = *lane.at(i);
            if (!body.IsInvalid())
            {
                bi.RemoveBody(body);
                bi.DestroyBody(body);
            }
        }
    }
    m_impl->static_storage.clear();

    m_impl->world.reset();
    m_impl->job_system.reset();
    m_impl->temp_allocator.reset();

    if (JPH::Factory::sInstance)
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void
physics_system::clear_static_world()
{
    if (!m_impl->world || m_impl->static_world_body.IsInvalid())
    {
        return;
    }
    auto& bi = m_impl->world->GetBodyInterface();
    bi.RemoveBody(m_impl->static_world_body);
    bi.DestroyBody(m_impl->static_world_body);
    m_impl->static_world_body = {};
}

namespace
{
// Concatenate the meshes into a single Jolt MeshShape. Returns an error result
// (HasError()) if no usable triangles were supplied.
JPH::ShapeSettings::ShapeResult
build_mesh_shape(const std::vector<static_world_mesh>& meshes)
{
    JPH::VertexList all_verts;
    JPH::IndexedTriangleList all_tris;

    uint32_t vertex_base = 0;
    for (const auto& m : meshes)
    {
        if (m.indices.size() < 3 || m.vertices.empty())
        {
            continue;
        }

        all_verts.reserve(all_verts.size() + m.vertices.size());
        for (const auto& v : m.vertices)
        {
            all_verts.push_back(JPH::Float3(v.x, v.y, v.z));
        }

        const auto tri_count = static_cast<uint32_t>(m.indices.size() / 3);
        all_tris.reserve(all_tris.size() + tri_count);
        for (uint32_t t = 0; t < tri_count; ++t)
        {
            all_tris.push_back(JPH::IndexedTriangle(vertex_base + m.indices[3 * t + 0],
                                                    vertex_base + m.indices[3 * t + 1],
                                                    vertex_base + m.indices[3 * t + 2]));
        }

        vertex_base += static_cast<uint32_t>(m.vertices.size());
    }

    if (all_tris.empty())
    {
        return {};  // empty -> HasError()
    }

    JPH::MeshShapeSettings mesh_settings(std::move(all_verts), std::move(all_tris));
    mesh_settings.SetEmbedded();
    return mesh_settings.Create();
}
}  // namespace

void
physics_system::build_static_world(const std::vector<static_world_mesh>& meshes)
{
    if (!m_impl->world)
    {
        return;
    }

    clear_static_world();

    auto shape_result = build_mesh_shape(meshes);
    if (shape_result.HasError())
    {
        return;
    }

    JPH::BodyCreationSettings bcs(shape_result.Get(),
                                  JPH::RVec3::sZero(),
                                  JPH::Quat::sIdentity(),
                                  JPH::EMotionType::Static,
                                  jolt_layers::NON_MOVING);

    auto& bi = m_impl->world->GetBodyInterface();
    m_impl->static_world_body = bi.CreateAndAddBody(bcs, JPH::EActivation::DontActivate);

    m_impl->world->OptimizeBroadPhase();
}

utils::laned_storage<k_static_collider_kind, JPH::BodyID>&
physics_system::static_collider_storage()
{
    return m_impl->static_storage;
}

void
physics_system::create_static_mesh(static_body_handle h, const static_world_mesh& mesh)
{
    if (!m_impl->world || !h.valid())
    {
        return;
    }

    const utils::handle<k_static_collider_kind> ph{h.value};
    // Consumer-side growth: make the slot this handle indexes addressable here, on
    // the physics thread, then claim it for the handle's generation. Until a body
    // lands it holds the zero-initialised (invalid) BodyID, which unregister and the
    // shutdown sweep read as "nothing to destroy".
    m_impl->static_storage.grow_for(ph);
    m_impl->static_storage.set_generation(ph, ph.generation());

    auto shape_result = build_mesh_shape({mesh});
    if (shape_result.HasError())
    {
        ALOG_WARN("create_static_mesh: degenerate mesh ({} verts, {} indices) — no collider",
                  mesh.vertices.size(),
                  mesh.indices.size());
        return;  // slot stays occupied with an invalid BodyID
    }

    JPH::BodyCreationSettings bcs(shape_result.Get(),
                                  JPH::RVec3::sZero(),
                                  JPH::Quat::sIdentity(),
                                  JPH::EMotionType::Static,
                                  jolt_layers::NON_MOVING);

    auto& bi = m_impl->world->GetBodyInterface();
    JPH::BodyID body = bi.CreateAndAddBody(bcs, JPH::EActivation::DontActivate);
    if (body.IsInvalid())
    {
        return;
    }

    m_impl->world->OptimizeBroadPhase();

    *m_impl->static_storage.at(ph) = body;
    ALOG_INFO("create_static_mesh: collider {} added ({} tris)", h.value, mesh.indices.size() / 3);
}

void
physics_system::unregister_static_mesh(static_body_handle h)
{
    if (!m_impl->world || !h.valid())
    {
        return;
    }

    const utils::handle<k_static_collider_kind> ph{h.value};
    if (!m_impl->static_storage.valid(ph))
    {
        return;  // stale or already-freed
    }

    const JPH::BodyID body = *m_impl->static_storage.at(ph);
    if (!body.IsInvalid())
    {
        auto& bi = m_impl->world->GetBodyInterface();
        bi.RemoveBody(body);
        bi.DestroyBody(body);
    }
    m_impl->static_storage.reset(ph);  // invalid BodyID + shadow gen 0 (empty)
}

void
physics_system::build_ground_plane(float plane_y, float half_extent)
{
    if (!m_impl->world)
    {
        return;
    }

    clear_static_world();

    // Thin flat box. Top face sits at plane_y when the body center is
    // plane_y - half_thickness.
    constexpr float half_thickness = 0.5f;
    JPH::BoxShapeSettings box_settings(JPH::Vec3(half_extent, half_thickness, half_extent));
    box_settings.SetEmbedded();
    auto shape_result = box_settings.Create();
    if (shape_result.HasError())
    {
        return;
    }

    JPH::BodyCreationSettings bcs(shape_result.Get(),
                                  JPH::RVec3(0.0f, plane_y - half_thickness, 0.0f),
                                  JPH::Quat::sIdentity(),
                                  JPH::EMotionType::Static,
                                  jolt_layers::NON_MOVING);

    auto& bi = m_impl->world->GetBodyInterface();
    m_impl->static_world_body = bi.CreateAndAddBody(bcs, JPH::EActivation::DontActivate);

    m_impl->world->OptimizeBroadPhase();
}

void
physics_system::tick(float dt)
{
    if (!m_impl->world || dt <= 0.0f)
    {
        return;
    }

    // Clamp big spikes (debugger pauses, frame hitches). Feeding a large dt
    // straight into Jolt blows bodies through the floor.
    constexpr float max_dt = 0.1f;
    float step_dt = dt < max_dt ? dt : max_dt;

    m_impl->world->Update(step_dt, 1, m_impl->temp_allocator.get(), m_impl->job_system.get());

    if (m_destructibles)
    {
        m_destructibles->tick(step_dt);
    }
}

destructible_physics&
physics_system::destructibles()
{
    return *m_destructibles;
}

}  // namespace physics
}  // namespace kryga

#include "physics_bridge/physics_bridge.h"
#include "physics_bridge/physics_commands_common.h"

#include <global_state/global_state.h>
#include <physics/physics_system.h>
#include <core/reflection/reflection_type.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/game_object.h>

#include <utility>

namespace kryga
{

void
state_mutator__physics_bridge::set(gs::state& s)
{
    auto p = s.create_box<physics_bridge>("physics_bridge");
    s.m_physics_bridge = p;
}

void*
physics_bridge::alloc_cmd_raw(size_t size, size_t align)
{
    return glob::glob_state().getr_physics_system().input_queue.alloc_raw(size, align);
}

void
physics_bridge::enqueue_cmd(physics_cmd::physics_command_base* cmd)
{
    glob::glob_state().getr_physics_system().input_queue.enqueue(cmd);
}

kryga::result_code
physics_bridge::physics_cmd_build(root::smart_object& obj, bool sub_objects)
{
    // CDOs are shared, readonly templates — never carry physics bodies.
    if (obj.get_flags().default_obj)
    {
        return result_code::ok;
    }

    auto build_fn = obj.get_reflection()->physics_cmd_builder;
    if (!build_fn)
    {
        return result_code::ok;
    }

    reflection::type_context__physics_cmd_build ctx{this, &obj, sub_objects};
    return build_fn(ctx);
}

kryga::result_code
physics_bridge::physics_cmd_destroy(root::smart_object& obj, bool sub_objects)
{
    if (obj.get_flags().default_obj)
    {
        return result_code::ok;
    }

    auto destroy_fn = obj.get_reflection()->physics_cmd_destroyer;
    if (!destroy_fn)
    {
        return result_code::ok;
    }

    reflection::type_context__physics_cmd_build ctx{this, &obj, sub_objects};
    return destroy_fn(ctx);
}

kryga::result_code
physics_bridge::physics_cmd_transform(root::game_object_component& source)
{
    auto r = source.get_owner()->get_components(source.get_order_idx());

    for (auto& obj : r)
    {
        auto handler = obj.get_reflection()->physics_cmd_transform;
        if (!handler)
        {
            continue;
        }

        reflection::type_context__physics_cmd_build ctx{this, &obj};
        handler(ctx);
    }

    return result_code::ok;
}

// ============================================================================
// Common commands
// ============================================================================

void
register_static_collider_cmd::execute(physics_cmd::physics_exec_context& ctx)
{
    physics::static_world_mesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);
    ctx.ps.create_static_mesh(handle, mesh);
}

void
unregister_static_collider_cmd::execute(physics_cmd::physics_exec_context& ctx)
{
    ctx.ps.unregister_static_mesh(handle);
}

}  // namespace kryga

#include "render_bridge/render_bridge.h"
#include "render_bridge/render_commands_common.h"

#include <global_state/global_state.h>
#include <vulkan_render/render_system.h>
#include <core/reflection/reflection_type.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/game_object.h>

#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/kryga_render.h>

namespace kryga
{

void
state_mutator__render_bridge::set(gs::state& s)
{
    auto p = s.create_box<render_bridge>("render_bridge");
    s.m_render_bridge = p;
}

void*
render_bridge::alloc_cmd_raw(size_t size, size_t align)
{
    return glob::glob_state().getr_render().input_queue.alloc_raw(size, align);
}

void
render_bridge::enqueue_cmd(render_cmd::render_command_base* cmd)
{
    glob::glob_state().getr_render().input_queue.enqueue(cmd);
}

kryga::result_code
render_bridge::render_cmd_build(root::smart_object& obj, bool sub_objects)
{
    KRG_check(!obj.get_flags().default_obj, "CDOs must not be render-built");

    if (obj.get_state() == root::smart_object_state::render_ready)
    {
        return result_code::ok;
    }

    auto build_fn = obj.get_reflection()->render_cmd_builder;
    if (!build_fn)
    {
        obj.set_state(root::smart_object_state::render_ready);
        return result_code::ok;
    }

    obj.set_state(root::smart_object_state::render_preparing);

    get_dependency().build_node(&obj);

    reflection::type_context__render_cmd_build ctx{this, &obj, sub_objects};
    result_code rc = build_fn(ctx);

    obj.set_state(root::smart_object_state::render_ready);

    return rc;
}

kryga::result_code
render_bridge::render_cmd_destroy(root::smart_object& obj, bool sub_objects)
{
    // Class-default objects (CDOs) are shared, readonly templates referenced by
    // instances — never render-built. A type's CDO can be loaded on first use
    // mid-play and swept into a level rollback, and a runtime component's
    // recursive destroy can also reach a CDO it merely references. Skip rather
    // than assert: there are no render resources to free, and tearing into shared
    // state would corrupt surviving objects. (Package-owned shared assets are
    // kept out of the recursion by is_same_source in the per-type destroyers.)
    if (obj.get_flags().default_obj)
    {
        return result_code::ok;
    }

    if (obj.get_state() == root::smart_object_state::constructed)
    {
        return result_code::ok;
    }

    KRG_check(obj.get_state() == root::smart_object_state::render_ready, "Should not happen");

    auto destroy_fn = obj.get_reflection()->render_cmd_destroyer;
    if (!destroy_fn)
    {
        obj.set_state(root::smart_object_state::constructed);
        return result_code::ok;
    }

    obj.set_state(root::smart_object_state::render_preparing);

    reflection::type_context__render_cmd_build ctx{this, &obj, sub_objects};
    result_code rc = destroy_fn(ctx);

    obj.set_state(root::smart_object_state::constructed);

    return rc;
}

kryga::result_code
render_bridge::render_cmd_transform(root::game_object_component& source)
{
    auto r = source.get_owner()->get_components(source.get_order_idx());

    for (auto& obj : r)
    {
        auto handler = obj.get_reflection()->render_cmd_transform;
        if (!handler)
        {
            continue;
        }

        reflection::type_context__render_cmd_build ctx{this, &obj};
        handler(ctx);
    }

    return result_code::ok;
}

// ============================================================================
// Common commands
// ============================================================================

void
update_transform_cmd::execute(render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().objects.find_by_id(id);
    if (!object_data)
    {
        return;
    }

    object_data->gpu_data.model = transform;
    object_data->gpu_data.normal = normal_matrix;
    object_data->gpu_data.obj_pos = position;
    object_data->gpu_data.bounding_sphere_center = bounding_sphere_center;
    object_data->gpu_data.bounding_radius = bounding_radius;

    ctx.vr.schd_update_object(object_data);
}

void
set_outline_cmd::execute(render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().objects.find_by_id(id);
    if (!object_data)
    {
        // Object already destroyed (e.g. selection cleared during level unload).
        return;
    }

    object_data->outlined = outlined;
    ctx.vr.schd_update_object_queue(object_data);
}

}  // namespace kryga

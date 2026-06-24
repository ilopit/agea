#include "render_translator/render_translator.h"

#include <global_state/global_state.h>
#include <vulkan_render/render_system.h>
#include <vulkan_render/render_thread.h>         // KRG_check_model_thread
#include <vulkan_render/vulkan_render_device.h>  // frames_in_flight()
#include <core/reflection/reflection_type.h>
#include <core/subsystem_queues.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/game_object.h>

#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/kryga_render.h>

namespace kryga
{

void
state_mutator__render_translator::set(gs::state& s)
{
    auto p = s.create_box<render_translator>("render_translator");
    s.m_render_translator = p;
}

void*
render_translator::alloc_cmd_raw(size_t size, size_t align)
{
    return glob::glob_state().getr_subsystem_queues().render.alloc_raw(size, align);
}

void
render_translator::enqueue_cmd(render_cmd::render_command_base* cmd)
{
    glob::glob_state().getr_subsystem_queues().render.enqueue(cmd);
}

// --- Content render-resource allocation (model thread) --------------------

void
render_translator::connect()
{
    KRG_check_model_thread();
    // Bind each model-side allocator to its render-side storage + lane (bind
    // claims the lane; the storage pointer is a dispatch token, never called
    // through in steady state -- growth happens render-side at populate).
    // Meshes/materials share their storage with the renderer's SYSTEM
    // allocators, which own k_system_lane; content takes k_content_lane.
    // Bound via render_system.loader, not renderer.get_cache(): the loader
    // exists from render_system construction, so this is safe even before
    // renderer.init() binds its loader pointer.
    auto& rs = glob::glob_state().getr_render();
    m_meshes_alloc.bind(rs.loader.meshes_storage(), render::types::k_content_lane);
    m_materials_alloc.bind(rs.loader.materials_storage(), render::types::k_content_lane);
    m_textures_alloc.bind(rs.loader.textures_storage(), 0);
    m_objects_alloc.bind(rs.loader.objects_storage(), 0);
    m_dir_lights_alloc.bind(rs.loader.dir_lights_storage(), 0);
    m_uni_lights_alloc.bind(rs.loader.uni_lights_storage(), 0);
    m_ui_texts_alloc.bind(rs.loader.ui_texts_storage(), 0);
}

void
render_translator::disconnect()
{
    // [shutdown, single-threaded] Direct detach is the sanctioned same-thread
    // form: the render loop is gone, so calling the storage is legal here.
    m_meshes_alloc.detach();
    m_materials_alloc.detach();
    m_textures_alloc.detach();
    m_objects_alloc.detach();
    m_dir_lights_alloc.detach();
    m_uni_lights_alloc.detach();
    m_ui_texts_alloc.detach();
}

void
render_translator::on_frame()
{
    KRG_check_model_thread();
    // Re-sync the deferral window to the GPU horizon each frame. The render lib
    // owns frames_in_flight; we pull it here (model thread) rather than have the
    // render side push it, which would invert the bridge->render dependency.
    const uint32_t fif = glob::glob_state().getr_render().device.frames_in_flight();
    m_meshes_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);
    m_materials_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);
    m_textures_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);
    m_objects_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);
    m_dir_lights_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);
    m_uni_lights_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);
    m_ui_texts_alloc.set_defer_ticks(static_cast<uint64_t>(fif) + 1);

    m_meshes_alloc.tick();
    m_materials_alloc.tick();
    m_textures_alloc.tick();
    m_objects_alloc.tick();
    m_dir_lights_alloc.tick();
    m_uni_lights_alloc.tick();
    m_ui_texts_alloc.tick();
}

kryga::result_code
render_translator::render_cmd_build(root::smart_object& obj, bool sub_objects)
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

    reflection::type_context__render_cmd_build ctx{.rb = this, .obj = &obj, .flag = sub_objects};
    result_code rc = build_fn(ctx);

    obj.set_state(root::smart_object_state::render_ready);

    return rc;
}

kryga::result_code
render_translator::render_cmd_destroy(root::smart_object& obj, bool sub_objects)
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

    reflection::type_context__render_cmd_build ctx{.rb = this, .obj = &obj, .flag = sub_objects};
    result_code rc = destroy_fn(ctx);

    obj.set_state(root::smart_object_state::constructed);

    return rc;
}

kryga::result_code
render_translator::render_cmd_transform(root::game_object_component& source)
{
    auto r = source.get_owner()->get_components(source.get_order_idx());

    for (auto& obj : r)
    {
        auto handler = obj.get_reflection()->render_cmd_transform;
        if (!handler)
        {
            continue;
        }

        reflection::type_context__render_cmd_build ctx{.rb = this, .obj = &obj};
        handler(ctx);
    }

    return result_code::ok;
}

}  // namespace kryga

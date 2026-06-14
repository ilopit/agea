#pragma once

#include "render_translator/render_dependency.h"
#include "render_translator/render_command.h"

#include <utils/id.h>
#include <utils/check.h>

#include <core/model_minimal.h>

#include <render_types/render_handle.h>

#include <new>

namespace kryga
{
namespace root
{
class smart_object;
class game_object_component;
}  // namespace root

// render_translator owns the stateful render-command lifecycle: it builds/destroys/
// transforms render commands from model objects and tracks their dependencies.
// Stateless model→render translation (create-infos, queue ids, GPU packing,
// sampler mapping) lives in render_convert.h, not here.
class render_translator
{
public:
    kryga::result_code
    render_cmd_build(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    render_cmd_destroy(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    render_cmd_transform(root::game_object_component& source);

    render_object_dependency_graph&
    get_dependency()
    {
        return m_dependency_graph;
    }

    // Raw allocation from the per-frame arena (implemented in .cpp)
    static void*
    alloc_cmd_raw(size_t size, size_t align);

    // Convenience: allocate a command from the per-frame arena
    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args);

    // Convenience: enqueue a command for render thread
    void
    enqueue_cmd(render_cmd::render_command_base* cmd);

    // --- Content render-resource allocators (MODEL THREAD) ----------------
    // The model owns slot allocation; the render-side loader/cache owns the
    // matching storage. The allocators are NULLABLE (lane_allocator's unbound
    // state): they sit unbound until bind_content_storages() claims each one's
    // storage lane -- minting on an unbound allocator asserts. Callers operate
    // the allocator directly: `rb->meshes_alloc().reserve()`,
    // `rb->objects_alloc().free(h)`. Handles flow to the render thread through
    // the command queue; the loader's storage populates by handle.
    render::types::mesh_allocator&
    meshes_alloc()
    {
        return m_meshes_alloc;
    }
    render::types::material_allocator&
    materials_alloc()
    {
        return m_materials_alloc;
    }
    render::types::texture_allocator&
    textures_alloc()
    {
        return m_textures_alloc;
    }
    render::types::render_object_allocator&
    objects_alloc()
    {
        return m_objects_alloc;
    }
    render::types::directional_light_allocator&
    dir_lights_alloc()
    {
        return m_dir_lights_alloc;
    }
    render::types::universal_light_allocator&
    uni_lights_alloc()
    {
        return m_uni_lights_alloc;
    }

    // [init, model thread] Bind each allocator to its render-side storage +
    // lane (bind claims the lane). Call once after render_system exists (the
    // loader storages live on it) and before any reserve/preallocate.
    // Implemented in the .cpp because it reaches into the render lib (the
    // loader).
    void
    bind_content_storages();

    // [shutdown, single-threaded] Release every lane claim; the allocators
    // return to the unbound state. Must run BEFORE the render system (and its
    // storages) is destroyed -- the storage dtor asserts no allocator is
    // still attached.
    void
    detach_content_storages();

    // Mature deferred frees in all content allocators. Call once per frame from the
    // producer (frame_pipeline::begin_frame). Re-syncs the deferral window to the
    // device's frames_in_flight so it tracks the GPU-resource delete queue.
    void
    tick_content_allocators();

private:
    render_object_dependency_graph m_dependency_graph;
    render::types::mesh_allocator m_meshes_alloc;
    render::types::material_allocator m_materials_alloc;
    render::types::texture_allocator m_textures_alloc;
    render::types::render_object_allocator m_objects_alloc;
    render::types::directional_light_allocator m_dir_lights_alloc;
    render::types::universal_light_allocator m_uni_lights_alloc;
};

template <typename T, typename... Args>
T*
render_translator::alloc_cmd(Args&&... args)
{
    void* mem = alloc_cmd_raw(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

}  // namespace kryga

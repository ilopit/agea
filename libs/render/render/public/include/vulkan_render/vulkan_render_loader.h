#pragma once

#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/utils/vulkan_image.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"
#include "vulkan_render/render_thread.h"  // KRG_check_model_thread / _render_thread

#include <error_handling/error_handling.h>

#include <utils/buffer.h>
#include <utils/id.h>
#include <utils/path.h>
#include <utils/check.h>
#include <utils/dynamic_object.h>
#include <utils/line_container.h>
#include <utils/path.h>
#include <render_types/render_handle.h>
#include <render_types/handle_pool.h>

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <queue>

namespace kryga
{
namespace render
{
class render_device;

// Per-level lightmap binding, owned render-side. The model level holds only the
// source rids; the bindless atlas index (produced by the render thread on level
// load) and the per-object UV transform live here so the model never caches a
// render handle. Flattened from core::lightmap_manifest at build time — only
// scale/offset + the index are needed at draw time, which keeps the render lib
// free of a core dependency (no core::lightmap_manifest in render headers).
struct lightmap_uv
{
    glm::vec2 scale{1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f};
};

struct lightmap_binding
{
    uint32_t bindless_index = 0xFFFFFFFFu;
    // The atlas texture itself — re-bakes update it in place (same bindless
    // slot, objects keep their index); remove_lightmap releases it.
    texture_data* texture = nullptr;
    std::unordered_map<kryga::utils::id, lightmap_uv> entries;  // object/component id → UV
};

// The storage instantiations the loader owns are the laned_storage aliases
// from render_types/render_handle.h (one lane per allocator; handles
// self-route by the lane bits in their index). Lane convention is defined
// there too: meshes/materials are ONE storage each with the renderer's system
// allocator on k_system_lane and render_bridge's content allocator on
// k_content_lane; every other pool is single-lane.
using object_storage = render::types::object_storage;
using dir_light_storage = render::types::dir_light_storage;
using uni_light_storage = render::types::uni_light_storage;
using mesh_storage = render::types::mesh_storage;
using material_storage = render::types::material_storage;
using texture_storage = render::types::bindless_texture_storage;
using texture_ref_storage = render::types::texture_ref_storage;

// Merged render-resource owner: asset/content storage (meshes, materials,
// textures, shaders, samplers, render passes) AND per-instance scene storage
// (objects, lights, bindless texture pool — formerly render_cache). One
// render-thread storage class; slot identity for content/scene pools is owned
// by the model-thread allocators in render_bridge, system pools by the
// renderer's allocators.
class vulkan_render_loader
{
public:
    /*************************/

    // --- Per-instance render objects (formerly render_cache) --------------
    // Addressed by render_object_handle. Slot identity (free-list + generation)
    // is owned by lane_allocator on the MODEL thread (render_bridge); this is
    // the render-thread STORAGE with a generation shadow for stale detection.
    // Lock-free: the model thread only touches the allocator, the render thread
    // is the sole reader/writer of the storage (growth included -- populate
    // paths grow_for the handle at execute time).

    // [init/model thread] render_bridge's object allocator claims lane 0 of
    // this at construction; preallocate pre-grows it (init-time, sanctioned).
    object_storage&
    objects_storage()
    {
        return m_objects;
    }

    // [render thread] Store a fresh render_data at a handle's slot. The handle's
    // index becomes slot()/the GPU SSBO index (single lane -> index is compact).
    vulkan_render_data*
    create_object(render::types::render_object_handle h, const kryga::utils::id& id)
    {
        KRG_check_render_thread_dbg();
        m_objects.grow_for(h);
        auto* slot = m_objects.at(h);
        *slot = vulkan_render_data(id, h.index());
        m_objects.set_generation(h, h.generation());
        return slot;
    }

    // [render thread] Resolve a handle to its render_data, or null if the slot
    // is empty/stale (handle freed out from under an in-flight command).
    vulkan_render_data*
    get_object(render::types::render_object_handle h)
    {
        KRG_check_render_thread_dbg();
        return m_objects.valid(h) ? m_objects.at(h) : nullptr;
    }

    // [render thread] Linear scan by id. Introspection only (RPC).
    vulkan_render_data*
    find_object_by_id(const kryga::utils::id& id)
    {
        KRG_check_render_thread_dbg();
        auto& lane = m_objects.lane(0);
        for (uint32_t i = 0; i < uint32_t(lane.size()); ++i)
        {
            if (lane.occupied(i) && lane.at(i)->id() == id)
            {
                return lane.at(i);
            }
        }
        return nullptr;
    }

    // [render thread] Mark a slot stale; payload stays in place for in-flight
    // frames (the allocator's deferred free keeps the index out of circulation
    // until they drain). Objects assert on a stale handle: that's a
    // double-retire bug, unlike lights where destroy may race a reload.
    void
    retire_object(render::types::render_object_handle h)
    {
        KRG_check_render_thread_dbg();
        KRG_check(m_objects.valid(h), "retire of a stale or null render-object handle");
        m_objects.set_generation(h, 0);
    }

    template <typename Fn>
    void
    for_each_object(Fn&& fn)
    {
        KRG_check_render_thread_dbg();
        auto& lane = m_objects.lane(0);
        for (uint32_t i = 0; i < uint32_t(lane.size()); ++i)
        {
            if (lane.occupied(i))
            {
                fn(*lane.at(i));
            }
        }
    }

    // Slot capacity (max index + 1). Sizes the object SSBO and the cull dispatch.
    uint64_t
    objects_capacity() const
    {
        return m_objects.size();
    }

    // Live object count (introspection / published stat).
    uint64_t
    objects_active() const
    {
        return m_objects.active();
    }

    void
    clear_objects()
    {
        KRG_check_render_thread_dbg();
        m_objects.clear();
    }

    // --- Directional lights (same storage shape as the object slot) ------------
    // render_bridge's allocator claims lane 0; index() == GPU SSBO slot.
    dir_light_storage&
    dir_lights_storage()
    {
        return m_dir_lights;
    }

    vulkan_directional_light_data*
    populate_dir_light(render::types::directional_light_handle h, const kryga::utils::id& id)
    {
        KRG_check_render_thread_dbg();
        m_dir_lights.grow_for(h);
        auto* slot = m_dir_lights.at(h);
        *slot = vulkan_directional_light_data(id, h.index());
        m_dir_lights.set_generation(h, h.generation());
        return slot;
    }

    vulkan_directional_light_data*
    get_dir_light(render::types::directional_light_handle h)
    {
        KRG_check_render_thread_dbg();
        return m_dir_lights.valid(h) ? m_dir_lights.at(h) : nullptr;
    }
    // Stale/empty handles tolerated (no assert): destroy can race a level reload.
    void
    retire_dir_light(render::types::directional_light_handle h)
    {
        KRG_check_render_thread_dbg();
        if (!m_dir_lights.valid(h))
        {
            return;
        }
        m_dir_lights.set_generation(h, 0);
    }
    // By GPU slot (== handle index); null if the slot is empty/retired.
    vulkan_directional_light_data*
    dir_light_at(uint32_t slot)
    {
        KRG_check_render_thread_dbg();
        auto& lane = m_dir_lights.lane(0);
        return (slot < lane.size() && lane.occupied(slot)) ? lane.at(slot) : nullptr;
    }

    uint64_t
    dir_lights_size() const
    {
        return m_dir_lights.size();
    }

    uint64_t
    dir_lights_active() const
    {
        return m_dir_lights.active();
    }

    template <typename Fn>
    void
    for_each_dir_light(Fn&& fn)
    {
        KRG_check_render_thread_dbg();
        auto& lane = m_dir_lights.lane(0);
        for (uint32_t i = 0; i < uint32_t(lane.size()); ++i)
        {
            if (lane.occupied(i))
            {
                fn(*lane.at(i));
            }
        }
    }

    void
    clear_dir_lights()
    {
        KRG_check_render_thread_dbg();
        m_dir_lights.clear();
    }

    // --- Universal lights (point + spot) ---------------------------------------
    uni_light_storage&
    uni_lights_storage()
    {
        return m_uni_lights;
    }

    vulkan_universal_light_data*
    populate_uni_light(render::types::universal_light_handle h,
                       const kryga::utils::id& id,
                       light_type type)
    {
        KRG_check_render_thread_dbg();
        m_uni_lights.grow_for(h);
        auto* slot = m_uni_lights.at(h);
        *slot = vulkan_universal_light_data(id, h.index(), type);
        m_uni_lights.set_generation(h, h.generation());
        return slot;
    }

    vulkan_universal_light_data*
    get_uni_light(render::types::universal_light_handle h)
    {
        KRG_check_render_thread_dbg();
        return m_uni_lights.valid(h) ? m_uni_lights.at(h) : nullptr;
    }

    void
    retire_uni_light(render::types::universal_light_handle h)
    {
        KRG_check_render_thread_dbg();
        if (!m_uni_lights.valid(h))
        {
            return;
        }
        m_uni_lights.set_generation(h, 0);
    }

    vulkan_universal_light_data*
    uni_light_at(uint32_t slot)
    {
        KRG_check_render_thread_dbg();
        auto& lane = m_uni_lights.lane(0);
        return (slot < lane.size() && lane.occupied(slot)) ? lane.at(slot) : nullptr;
    }

    uint64_t
    uni_lights_size() const
    {
        return m_uni_lights.size();
    }

    uint64_t
    uni_lights_active() const
    {
        return m_uni_lights.active();
    }

    template <typename Fn>
    void
    for_each_uni_light(Fn&& fn)
    {
        KRG_check_render_thread_dbg();
        auto& lane = m_uni_lights.lane(0);
        for (uint32_t i = 0; i < uint32_t(lane.size()); ++i)
        {
            if (lane.occupied(i))
            {
                fn(*lane.at(i));
            }
        }
    }

    void
    clear_uni_lights()
    {
        KRG_check_render_thread_dbg();
        m_uni_lights.clear();
    }

    // --- Bindless texture pool (formerly render_cache) -------------------------
    // The slot index IS the GPU bindless descriptor index (texture_data::slot()).
    // STORAGE only: slot identity (reserve/reclaim) is owned by the renderer
    // (alloc_texture / release_texture on vulkan_render); this class only
    // initializes, fills and resets the payload at a renderer-reserved slot.
    // There is NO id index: every owner (renderer system textures, content
    // handle storage, lightmap binding, previewer) holds its texture_data*.
    texture_storage&
    bindless_textures_storage()
    {
        return m_textures;
    }
    // [render thread] Initialize a renderer-reserved slot's payload (value +
    // generation + handle).
    texture_data*
    init_texture_slot(render::types::texture_handle h, const kryga::utils::id& id);
    // [render thread] Destruct the payload in place + mark the slot empty. The
    // renderer reclaims the slot afterwards.
    void
    reset_texture_slot(texture_data* td);
    // [render thread] Upload image data into a texture (staging + view). Does
    // NOT touch the bindless descriptor queue — the renderer stages the update.
    void
    fill_texture(texture_data* td,
                 const kryga::utils::buffer& data,
                 uint32_t w,
                 uint32_t h,
                 VkFormat vk_format,
                 texture_format fmt);
    uint64_t
    textures_active() const
    {
        return m_textures.active();
    }
    void
    clear_textures();

    /*************************/

    // --- Storage accessors (for allocator construction) -------------------
    // Allocators claim a lane at construction: render_bridge's content
    // allocators take k_content_lane on meshes/materials (and lane 0 on the
    // single-lane pools); the renderer's system allocators take k_system_lane
    // of the SAME merged mesh/material storages.
    mesh_storage&
    meshes_storage()
    {
        return m_meshes_storage;
    }
    material_storage&
    materials_storage()
    {
        return m_materials_storage;
    }
    texture_ref_storage&
    textures_storage()
    {
        return m_textures_storage;
    }
    // System mesh/material accessors: SAME storages as the content ones above
    // (merged; the lane id in the handle routes). Kept as named entry points
    // for the bind sites' readability.
    mesh_storage&
    system_meshes_storage()
    {
        return m_meshes_storage;
    }
    material_storage&
    system_materials_storage()
    {
        return m_materials_storage;
    }

    // Re-stamp every storage's consumer-thread binding at a lifecycle handoff:
    // init populates on the main thread, the render thread consumes in steady
    // state (populate/at/reset/grow_for at command drain), main reclaims for
    // teardown. Called by the renderer's bind_render_pools_to_current_thread.
    void
    bind_storages_to_current_thread()
    {
        m_meshes_storage.bind_to_current_thread();
        m_materials_storage.bind_to_current_thread();
        m_textures_storage.bind_to_current_thread();
        m_textures.bind_to_current_thread();
        m_objects.bind_to_current_thread();
        m_dir_lights.bind_to_current_thread();
        m_uni_lights.bind_to_current_thread();
    }

    // --- Content meshes (game objects) — lock-free, storage side ----------
    // The CONTENT pool is split across libs: render_bridge owns the allocator
    // (reserve / free / tick — model thread); this loader owns the STORAGE
    // (populate / get / valid / reset — render thread). They share no mutable
    // state, only the handle value passed through the command queue, so neither
    // path takes a lock. The storage carries a generation shadow for validation.
    mesh_data*
    get_mesh_data(render::types::mesh_handle h)
    {
        KRG_check_render_thread_dbg();  // storage is render-owned
        KRG_check_debug(m_meshes_storage.valid(h), "deref of a stale/null mesh handle");
        return m_meshes_storage.at(h);
    }

    bool
    mesh_valid(render::types::mesh_handle h) const
    {
        KRG_check_render_thread_dbg();
        return m_meshes_storage.valid(h);
    }

    // [render thread] Fill a pre-reserved CONTENT mesh slot and mark it resident.
    void
    populate_mesh(render::types::mesh_handle h,
                  const kryga::utils::id& mesh_id,
                  kryga::utils::buffer_view<gpu::vertex_data> vertices,
                  kryga::utils::buffer_view<gpu::uint> indices);

    void
    populate_skinned_mesh(render::types::mesh_handle h,
                          const kryga::utils::id& mesh_id,
                          kryga::utils::buffer_view<gpu::skinned_vertex_data> vertices,
                          kryga::utils::buffer_view<gpu::uint> indices);

    // [render thread] Release a content mesh slot's GPU data + invalidate it.
    void
    reset_mesh_storage(render::types::mesh_handle h);

    // --- System meshes (fullscreen quad, debug meshes, test resources) ----
    // Render-owned SYSTEM pool, storage side only. Lifecycle (reserve/reclaim
    // + the public create_mesh/destroy_system_mesh_data API) lives on
    // vulkan_render, which owns the matching allocator; these primitives build
    // and store at a renderer-reserved slot.
    mesh_data*
    populate_system_mesh(render::types::mesh_handle h,
                         const kryga::utils::id& mesh_id,
                         kryga::utils::buffer_view<gpu::vertex_data> vertices,
                         kryga::utils::buffer_view<gpu::uint> indices);

    mesh_data*
    populate_system_skinned_mesh(render::types::mesh_handle h,
                                 const kryga::utils::id& mesh_id,
                                 kryga::utils::buffer_view<gpu::skinned_vertex_data> vertices,
                                 kryga::utils::buffer_view<gpu::uint> indices);

    mesh_data*
    system_mesh_at(render::types::mesh_handle h)
    {
        KRG_check_render_thread_dbg();
        return m_meshes_storage.at(h);  // merged storage; the handle's lane routes
    }

    void
    reset_system_mesh(render::types::mesh_handle h)
    {
        KRG_check_render_thread_dbg();
        m_meshes_storage.reset(h);
    }

    /*************************/

    // --- Content textures (game-object materials) — handle path -----------
    // The bridge owns the allocator; this loader maps handle -> texture_data*.
    // The texture_data itself lives in the bindless pool above (m_textures),
    // so the handle is a build-time reference that resolves to a bindless index;
    // at draw time materials carry only that bindless index, not the handle.
    texture_data*
    get_texture_data(render::types::texture_handle h)
    {
        KRG_check_render_thread_dbg();
        // Stale/unpopulated -> null (a material slot may reference a texture that
        // was freed across a reload; the caller checks for null and skips it).
        if (!m_textures_storage.valid(h))
        {
            return nullptr;
        }
        return *m_textures_storage.at(h);
    }

    bool
    texture_valid(render::types::texture_handle h) const
    {
        return m_textures_storage.valid(h);
    }

    // [render thread] Build a content texture into the bindless cache + map handle.
    void
    populate_texture(render::types::texture_handle h,
                     const kryga::utils::id& texture_id,
                     const kryga::utils::buffer& base_color,
                     uint32_t w,
                     uint32_t height);

    // [render thread] Release a content texture's bindless slot + invalidate it.
    void
    reset_texture_storage(render::types::texture_handle h);

    /*************************/
    // Per-level lightmap registry (render-thread owned). create_lightmap_cmd
    // writes on level load; object build commands read it at execute time to
    // resolve each instance's lightmap UV + atlas index. Keyed by level id.
    // The binding OWNS its atlas texture: set_lightmap takes the texture so a
    // re-bake can update it in place, remove_lightmap releases it on unload.
    void
    set_lightmap(const kryga::utils::id& level_id,
                 texture_data* texture,
                 std::unordered_map<kryga::utils::id, lightmap_uv> entries);

    const lightmap_binding*
    get_lightmap(const kryga::utils::id& level_id) const
    {
        KRG_check_render_thread_dbg();
        auto itr = m_lightmaps.find(level_id);
        return itr != m_lightmaps.end() ? &itr->second : nullptr;
    }

    void
    remove_lightmap(const kryga::utils::id& level_id);

    /*************************/

    bool
    update_object(vulkan_render_data& obj_data,
                  material_data& mat_data,
                  mesh_data& mesh_data,
                  const glm::mat4& model_matrix,
                  const glm::mat4& normal_matrix,
                  const glm::vec3& obj_pos);

    /*************************/

    // --- Content materials (game objects) — storage side ------------------
    // Same split as content meshes: render_bridge owns the allocator, this loader
    // owns the storage.
    material_data*
    get_material_data(render::types::material_handle h)
    {
        KRG_check_render_thread_dbg();  // storage is render-owned
        KRG_check_debug(m_materials_storage.valid(h), "deref of a stale/null material handle");
        return m_materials_storage.at(h);
    }

    bool
    material_valid(render::types::material_handle h) const
    {
        KRG_check_render_thread_dbg();
        return m_materials_storage.valid(h);
    }

    // [render thread]
    void
    populate_material(render::types::material_handle h,
                      const kryga::utils::id& id,
                      const kryga::utils::id& type_id,
                      std::vector<texture_sampler_data>& textures_data,
                      shader_effect_data& se_data,
                      const kryga::utils::dynobj& params);

    // [render thread] Release a content material slot's data + invalidate it.
    void
    reset_material_storage(render::types::material_handle h);

    bool
    update_material(material_data& mat_data,
                    std::vector<texture_sampler_data>& textures_data,
                    shader_effect_data& se_data,
                    const kryga::utils::dynobj& params);

    // --- System materials (UI/grid/outline/upscale, test resources) -------
    // Render-owned SYSTEM pool, storage side only — symmetric to system
    // meshes. Lifecycle (create_material / destroy_system_material_data) lives
    // on vulkan_render.
    material_data*
    populate_system_material(render::types::material_handle h,
                             const kryga::utils::id& id,
                             const kryga::utils::id& type_id,
                             std::vector<texture_sampler_data>& textures_data,
                             shader_effect_data& se_data,
                             const kryga::utils::dynobj& params);

    material_data*
    system_material_at(render::types::material_handle h)
    {
        KRG_check_render_thread_dbg();
        return m_materials_storage.at(h);  // merged storage; the handle's lane routes
    }

    void
    reset_system_material(render::types::material_handle h)
    {
        KRG_check_render_thread_dbg();
        m_materials_storage.reset(h);
    }

    /*************************/
    render_pass*
    get_render_pass(const kryga::utils::id& id)
    {
        KRG_check_render_thread_dbg();
        auto itr = m_render_passes.find(id);
        return itr != m_render_passes.end() ? itr->second.get() : nullptr;
    }

    void
    add_render_pass(const kryga::utils::id& id, render_pass_sptr rp)
    {
        KRG_check_render_thread_dbg();
        m_render_passes[id] = std::move(rp);
    }

    void
    destroy_render_pass(const kryga::utils::id& id);

    /*************************/
    void
    clear_caches();

private:
    // --- Per-instance scene storage (identity in render_bridge) ---------------
    // Single-lane: the lone allocator (render_bridge) claims lane 0, so handle
    // index == GPU SSBO slot stays compact.
    object_storage m_objects{1};
    dir_light_storage m_dir_lights{1};
    uni_light_storage m_uni_lights{1};
    // Bindless texture pool STORAGE; the matching allocator lives on the
    // renderer (bindless_textures_alloc). Single-lane: bindless slot == index.
    texture_storage m_textures{1};

    // MERGED mesh storage: the renderer's SYSTEM allocator (quad, debug/test
    // meshes) claims k_system_lane, render_bridge's CONTENT allocator claims
    // k_content_lane. One draw-path entry point; handles self-route by their
    // lane bits, the producers share no state. This loader only ever touches
    // storage (populate/get/valid/reset + grow_for, render thread).
    mesh_storage m_meshes_storage{2};
    // Content texture handle -> texture_data* (non-owning; the data lives in the
    // bindless pool m_textures). Single-lane: only render_bridge allocates here.
    texture_ref_storage m_textures_storage{1};
    // MERGED material storage, same split as the mesh pool above.
    material_storage m_materials_storage{2};
    std::unordered_map<kryga::utils::id, render_pass_sptr> m_render_passes;

    std::unordered_map<kryga::utils::id, lightmap_binding> m_lightmaps;
};

}  // namespace render
}  // namespace kryga

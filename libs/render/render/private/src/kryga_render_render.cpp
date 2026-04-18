#include "vulkan_render/kryga_render.h"

#include <tracy/Tracy.hpp>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_render_data.h"

#include <gpu_types/gpu_generic_constants.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <imgui.h>

#include <vfs/vfs.h>
#include <vfs/io.h>
#include <global_state/global_state.h>

#include <algorithm>

namespace kryga
{
namespace render
{

namespace
{

// Helper to copy material's bindless texture and sampler indices to push constants
void
copy_texture_indices(gpu::push_constants_main& config, const material_data* mat)
{
    // Initialize all slots to invalid/default
    for (uint32_t i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        config.texture_indices[i] = UINT32_MAX;
        config.sampler_indices[i] = KGPU_SAMPLER_LINEAR_REPEAT;  // Default sampler
    }

    if (!mat)
    {
        return;
    }

    // Copy bindless texture indices from material
    const auto& tex_indices = mat->get_bindless_texture_indices();
    static bool logged = false;
    if (!logged && !tex_indices.empty())
    {
        ALOG_INFO("copy_texture_indices: mat {} has {} indices, first={}",
                  mat->get_id().cstr(),
                  tex_indices.size(),
                  tex_indices[0]);
        logged = true;
    }
    for (uint32_t i = 0; i < tex_indices.size() && i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        config.texture_indices[i] = tex_indices[i];
    }

    // Copy bindless sampler indices from material
    const auto& sampler_indices = mat->get_bindless_sampler_indices();
    for (uint32_t i = 0; i < sampler_indices.size() && i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        config.sampler_indices[i] = sampler_indices[i];
    }
}

}  // namespace

// ============================================================================
// Instance Drawing Functions
// ============================================================================

void
vulkan_render::upload_instance_slots(render::frame_state& frame)
{
    if (m_instance_slots_staging.empty())
    {
        return;
    }

    const size_t required_size = m_instance_slots_staging.size() * sizeof(uint32_t);

    // Regrow if needed (same pattern as ensure_buffer_capacity_and_map)
    if (required_size >= frame.buffers.instance_slots.get_alloc_size())
    {
        auto old_buffer = std::move(frame.buffers.instance_slots);
        frame.buffers.instance_slots = glob::glob_state().getr_render_device().create_buffer(
            required_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        ALOG_INFO("Reallocating instance_slots buffer {} => {}",
                  old_buffer.get_alloc_size(),
                  frame.buffers.instance_slots.get_alloc_size());
    }

    frame.buffers.instance_slots.begin();
    auto* dst = (uint32_t*)frame.buffers.instance_slots.allocate_data((uint32_t)required_size);
    memcpy(dst, m_instance_slots_staging.data(), required_size);
    frame.buffers.instance_slots.end();
}

void
vulkan_render::build_batches_for_queue(render_line_container& r, bool outlined)
{
    build_batches_for_queue_into(r, outlined, m_draw_batches);
}

void
vulkan_render::build_batches_for_queue_into(render_line_container& r,
                                            bool outlined,
                                            std::vector<draw_batch>& out_batches)
{
    if (r.empty())
    {
        return;
    }

    mesh_data* cur_mesh = nullptr;
    bool cur_cast_shadows = true;
    uint32_t batch_start = (uint32_t)m_instance_slots_staging.size();

    for (auto& obj : r)
    {
        ++m_all_draws;

        // Frustum culling - happens here now, not in draw
        if (m_render_config.debug.frustum_culling &&
            !m_frustum.is_sphere_visible(obj->gpu_data.obj_pos, obj->gpu_data.bounding_radius))
        {
            ++m_culled_draws;
            continue;
        }

        // Mesh change = finalize previous batch
        if (cur_mesh && cur_mesh != obj->mesh)
        {
            uint32_t instance_count = (uint32_t)m_instance_slots_staging.size() - batch_start;
            if (instance_count > 0)
            {
                out_batches.push_back({.mesh = cur_mesh,
                                       .material = r.front()->material,
                                       .instance_count = instance_count,
                                       .first_instance_offset = batch_start,
                                       .outlined = outlined,
                                       .cast_shadows = cur_cast_shadows});
            }
            batch_start = (uint32_t)m_instance_slots_staging.size();
        }

        cur_mesh = obj->mesh;
        cur_cast_shadows = (obj->layer_flags & render::LAYER_CAST_SHADOWS) != 0;
        m_instance_slots_staging.push_back(obj->slot());
    }

    // Final batch
    if (cur_mesh)
    {
        uint32_t instance_count = (uint32_t)m_instance_slots_staging.size() - batch_start;
        if (instance_count > 0)
        {
            out_batches.push_back({.mesh = cur_mesh,
                                   .material = r.front()->material,
                                   .instance_count = instance_count,
                                   .first_instance_offset = batch_start,
                                   .outlined = outlined,
                                   .cast_shadows = cur_cast_shadows});
        }
    }
}

void
vulkan_render::prepare_instance_data(render::frame_state& frame)
{
    ZoneScopedN("Render::PrepareInstanceData");

    m_instance_slots_staging.clear();
    m_draw_batches.clear();
    m_debug_draw_batches.clear();

    // Build batches for default queue
    for (auto& [queue_id, container] : m_default_render_object_queue)
    {
        build_batches_for_queue(container, false);
    }

    // Build batches for outline queue
    for (auto& [queue_id, container] : m_outline_render_object_queue)
    {
        build_batches_for_queue(container, true);
    }

    // Build batches for debug queue (separate list — skipped by shadows)
    for (auto& [queue_id, container] : m_debug_render_object_queue)
    {
        build_batches_for_queue_into(container, false, m_debug_draw_batches);
    }

    // Upload all instance slots
    upload_instance_slots(frame);
}

// ============================================================================
// Draw Functions - Instanced Mode
// ============================================================================

void
vulkan_render::draw_objects_instanced(render::frame_state& current_frame)
{
    ZoneScopedN("Render::DrawObjectsInstanced");

    auto cmd = current_frame.frame->m_main_command_buffer;

    // Bind global descriptor sets once for the entire render pass
    bind_global_descriptors(cmd, current_frame);

    pipeline_ctx pctx{};
    material_data* cur_material = nullptr;
    bool cur_outlined = false;

    // Draw all batches (already frustum culled in prepare phase)
    for (const auto& batch : m_draw_batches)
    {
        // Rebind material/pipeline if changed
        if (cur_material != batch.material || cur_outlined != batch.outlined)
        {
            bind_material(cmd, batch.material, current_frame, pctx, batch.outlined);
            cur_material = batch.material;
            cur_outlined = batch.outlined;
        }

        bind_mesh(cmd, batch.mesh);

        // Push constants with instance_base and texture indices
        m_obj_config.instance_base = batch.first_instance_offset;
        m_obj_config.material_id = batch.material->gpu_idx();
        m_obj_config.use_clustered_lighting = 1;
        m_obj_config.directional_light_id = get_selected_directional_light_slot();
        copy_texture_indices(m_obj_config, batch.material);

        vkCmdPushConstants(cmd,
                           pctx.pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(gpu::push_constants_main),
                           &m_obj_config);

        // Instanced draw
        if (batch.mesh->has_indices())
        {
            vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
        }
    }

    // DEBUG — render with all lighting disabled (unlit)
    if (!m_debug_draw_batches.empty())
    {
        // Save lighting state
        uint32_t saved_dir = m_obj_config.enable_directional_light;
        uint32_t saved_local = m_obj_config.enable_local_lights;
        uint32_t saved_baked = m_obj_config.enable_baked_light;

        m_obj_config.enable_directional_light = 0;
        m_obj_config.enable_local_lights = 0;
        m_obj_config.enable_baked_light = 0;

        pctx = {};
        cur_material = nullptr;

        for (const auto& batch : m_debug_draw_batches)
        {
            if (cur_material != batch.material)
            {
                bind_material(cmd, batch.material, current_frame, pctx, false);
                cur_material = batch.material;
            }

            bind_mesh(cmd, batch.mesh);

            m_obj_config.instance_base = batch.first_instance_offset;
            m_obj_config.material_id = batch.material->gpu_idx();
            m_obj_config.use_clustered_lighting = 0;
            m_obj_config.directional_light_id = 0;
            copy_texture_indices(m_obj_config, batch.material);

            vkCmdPushConstants(cmd,
                               pctx.pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(gpu::push_constants_main),
                               &m_obj_config);

            if (batch.mesh->has_indices())
            {
                vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
            }
            else
            {
                vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
            }
        }

        // Restore lighting state
        m_obj_config.enable_directional_light = saved_dir;
        m_obj_config.enable_local_lights = saved_local;
        m_obj_config.enable_baked_light = saved_baked;
    }

    // TRANSPARENT - needs per-object sorting, add slots after opaque batches
    if (!m_transparent_render_object_queue.empty())
    {
        update_transparent_objects_queue();

        // Add transparent object slots to instance buffer (after opaque objects)
        uint32_t transparent_base = (uint32_t)m_instance_slots_staging.size();
        for (auto& obj : m_transparent_render_object_queue)
        {
            m_instance_slots_staging.push_back(obj->slot());
        }
        // Re-upload the instance slots buffer with transparent objects
        upload_instance_slots(current_frame);

        // Draw transparent objects with individual instance_base offsets
        mesh_data* cur_mesh = nullptr;
        pctx = {};
        uint32_t transparent_idx = 0;

        for (auto& obj : m_transparent_render_object_queue)
        {
            if (pctx.cur_material_type_idx != obj->material->gpu_type_idx())
            {
                bind_material(cmd, obj->material, current_frame, pctx);
            }
            else if (pctx.cur_material_idx != obj->material->gpu_idx())
            {
                pctx.cur_material_idx = obj->material->gpu_idx();
                // Note: With bindless textures, we don't need to rebind texture descriptor set
                // The global bindless set is already bound, texture indices come via push constants
            }

            if (cur_mesh != obj->mesh)
            {
                cur_mesh = obj->mesh;
                bind_mesh(cmd, cur_mesh);
            }

            // Set push constants for this transparent object
            m_obj_config.directional_light_id = m_cache.directional_lights.get_size() > 0
                                                    ? m_cache.directional_lights.at(0)->slot()
                                                    : 0;
            m_obj_config.use_clustered_lighting = 1;
            m_obj_config.material_id = obj->material->gpu_idx();
            m_obj_config.instance_base = transparent_base + transparent_idx;
            copy_texture_indices(m_obj_config, obj->material);

            vkCmdPushConstants(cmd,
                               pctx.pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(gpu::push_constants_main),
                               &m_obj_config);

            if (obj->mesh->has_indices())
            {
                vkCmdDrawIndexed(cmd, obj->mesh->indices_size(), 1, 0, 0, 0);
            }
            else
            {
                vkCmdDraw(cmd, obj->mesh->vertices_size(), 1, 0, 0);
            }

            ++transparent_idx;
        }
    }

    // Draw debug light visualization
    draw_debug_lights(cmd, current_frame);

    // Draw grid overlay
    draw_grid(cmd, current_frame);

    // Draw outline post-process (edge detection on selection mask)
    draw_outline_post(cmd, current_frame);

    // Draw UI overlay
    draw_ui_overlay(cmd, current_frame);
}

// ============================================================================
// Draw Functions - Picking
// ============================================================================
// Draw Functions - Grid (shared)
// ============================================================================

void
vulkan_render::draw_grid(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    auto m = glob::glob_state().getr_vulkan_render_loader().get_mesh_data(AID("plane_mesh"));
    if (!m_render_config.debug.show_grid || !m_grid_mat || !m_grid_se || !m)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_grid_se->m_pipeline);

    // Bind set 0 (camera UBO) — required by the grid vertex/fragment shader.
    if (m_main_global_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_grid_se->m_pipeline_layout,
                                KGPU_global_descriptor_sets,
                                1,
                                &m_main_global_set,
                                0,
                                nullptr);
    }

    bind_mesh(cmd, m);

    if (m->has_indices())
    {
        vkCmdDrawIndexed(cmd, m->indices_size(), 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(cmd, m->vertices_size(), 1, 0, 0);
    }
    (void)current_frame;
}

// ============================================================================
// Draw Functions - Selection Mask (outline pre-pass)
// ============================================================================

void
vulkan_render::draw_selection_mask(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    // Draw outlined objects using their own material pipelines.
    // This preserves billboard vertex transforms. The RGBA8 mask target
    // captures any non-zero output as "selected" (sampled as R channel).
    bind_global_descriptors(cmd, current_frame);

    pipeline_ctx pctx{};
    material_data* cur_material = nullptr;

    for (const auto& batch : m_draw_batches)
    {
        if (!batch.outlined)
        {
            continue;
        }

        if (cur_material != batch.material)
        {
            bind_material(cmd, batch.material, current_frame, pctx, false);
            cur_material = batch.material;
        }

        bind_mesh(cmd, batch.mesh);

        m_obj_config.instance_base = batch.first_instance_offset;
        m_obj_config.material_id = batch.material->gpu_idx();
        m_obj_config.use_clustered_lighting = 1;
        m_obj_config.directional_light_id = get_selected_directional_light_slot();
        copy_texture_indices(m_obj_config, batch.material);

        vkCmdPushConstants(cmd,
                           pctx.pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(gpu::push_constants_main),
                           &m_obj_config);

        if (batch.mesh->has_indices())
        {
            vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
        }
    }
}

// ============================================================================
// Draw Functions - Outline Post-Process
// ============================================================================

void
vulkan_render::draw_outline_post(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    KRG_check(m_outline_post_se, "Outline post shader effect not initialized");
    KRG_check(m_selection_mask_bindless_idx != 0xFFFFFFFFu, "Selection mask not bound to bindless");

    // Check if there are any outlined objects
    bool has_outlined = false;
    for (const auto& batch : m_draw_batches)
    {
        if (batch.outlined)
        {
            has_outlined = true;
            break;
        }
    }
    if (!has_outlined)
    {
        return;
    }

    auto m = glob::glob_state().getr_vulkan_render_loader().get_mesh_data(AID("plane_mesh"));
    if (!m)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_outline_post_se->m_pipeline);

    if (m_bindless_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_outline_post_se->m_pipeline_layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &m_bindless_set,
                                0,
                                nullptr);
    }

    bind_mesh(cmd, m);

    // Push constants: outline color, texel size, thickness, mask texture index
    struct outline_pc
    {
        glm::vec4 outline_color;
        glm::vec2 texel_size;
        float thickness;
        uint32_t mask_texture_idx;
    };

    const auto& sel = m_render_config.selection;
    outline_pc pc;
    pc.outline_color = glm::vec4(
        sel.outline_color[0], sel.outline_color[1], sel.outline_color[2], sel.outline_color[3]);
    pc.texel_size = glm::vec2(1.0f / m_width, 1.0f / m_height);
    pc.thickness = sel.outline_thickness;
    pc.mask_texture_idx = m_selection_mask_bindless_idx;

    vkCmdPushConstants(cmd,
                       m_outline_post_se->m_pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(outline_pc),
                       &pc);

    if (m->has_indices())
    {
        vkCmdDrawIndexed(cmd, m->indices_size(), 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(cmd, m->vertices_size(), 1, 0, 0);
    }
}

// ============================================================================
// Draw Functions - UI Overlay (shared)
// ============================================================================

void
vulkan_render::draw_ui_overlay(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    if (!m_ui_target_mat)
    {
        return;
    }

    auto m = glob::glob_state().getr_vulkan_render_loader().get_mesh_data(AID("plane_mesh"));
    if (!m)
    {
        return;
    }

    // UI overlay shader has a different (incompatible) pipeline layout for sets 0-1
    // It only uses set 2 (textures) and push constants, so we bind those directly
    auto pipeline_layout = m_ui_target_mat->get_shader_effect()->m_pipeline_layout;

    vkCmdBindPipeline(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_target_mat->get_shader_effect()->m_pipeline);

    // Rebind bindless textures with UI's compatible pipeline layout
    if (m_bindless_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &m_bindless_set,
                                0,
                                nullptr);
    }
    else if (m_ui_target_mat->has_textures())
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &m_ui_target_mat->get_textures_ds(),
                                0,
                                nullptr);
    }

    bind_mesh(cmd, m);

    // Push constants with texture indices for UI material
    copy_texture_indices(m_obj_config, m_ui_target_mat);
    vkCmdPushConstants(cmd,
                       pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(gpu::push_constants_main),
                       &m_obj_config);

    if (!m->has_indices())
    {
        vkCmdDraw(cmd, m->vertices_size(), 1, 0, 0);
    }
    else
    {
        vkCmdDrawIndexed(cmd, m->indices_size(), 1, 0, 0, 0);
    }
}

void
vulkan_render::draw_multi_pipeline_objects_queue(render_line_container& r,
                                                 VkCommandBuffer cmd,
                                                 render::frame_state& current_frame)
{
    mesh_data* cur_mesh = nullptr;

    pipeline_ctx pctx{};

    for (auto& obj : r)
    {
        ++m_all_draws;
        // Frustum culling
        if (m_render_config.debug.frustum_culling &&
            !m_frustum.is_sphere_visible(obj->gpu_data.obj_pos, obj->gpu_data.bounding_radius))
        {
            ++m_culled_draws;
            continue;
        }

        if (pctx.cur_material_type_idx != obj->material->gpu_type_idx())
        {
            bind_material(cmd, obj->material, current_frame, pctx);
        }
        else if (pctx.cur_material_idx != obj->material->gpu_idx())
        {
            pctx.cur_material_idx = obj->material->gpu_idx();
            // Note: With bindless textures, texture indices come via push constants
            // No need to rebind descriptor set for different materials
        }

        if (cur_mesh != obj->mesh)
        {
            cur_mesh = obj->mesh;
            bind_mesh(cmd, cur_mesh);
        }

        draw_object(cmd, pctx, obj);
    }
}

void
vulkan_render::draw_objects_queue(render_line_container& r,
                                  VkCommandBuffer cmd,
                                  render::frame_state& current_frame,
                                  bool outlined)

{
    pipeline_ctx pctx{};

    if (!r.empty())
    {
        bind_material(cmd, r.front()->material, current_frame, pctx, outlined);
    }

    draw_same_pipeline_objects_queue(cmd, pctx, r);
}

void
vulkan_render::draw_same_pipeline_objects_queue(VkCommandBuffer cmd,
                                                const pipeline_ctx& pctx,
                                                const render_line_container& r,
                                                bool rebind_images)
{
    mesh_data* cur_mesh = nullptr;
    // Note: rebind_images parameter is now obsolete with bindless textures
    // Texture indices are passed via push constants, not descriptor sets
    (void)rebind_images;

    for (auto& obj : r)
    {
        ++m_all_draws;
        // Frustum culling
        if (m_render_config.debug.frustum_culling &&
            !m_frustum.is_sphere_visible(obj->gpu_data.obj_pos, obj->gpu_data.bounding_radius))
        {
            ++m_culled_draws;
            continue;
        }

        if (cur_mesh != obj->mesh)
        {
            cur_mesh = obj->mesh;
            bind_mesh(cmd, cur_mesh);
        }

        draw_object(cmd, pctx, obj);
    }
}

void
vulkan_render::draw_object(VkCommandBuffer cmd,
                           const pipeline_ctx& pctx,
                           const render::vulkan_render_data* obj)
{
    // Legacy per-object drawing function
    // Uses identity buffer (slots[i] = i) with instance_base = 0 and firstInstance = slot
    // So get_object_index() = slots[0 + slot] = slot

    // Set directional light (global)
    m_obj_config.directional_light_id = get_selected_directional_light_slot();

    // Always use clustered lighting
    m_obj_config.use_clustered_lighting = 1;

    auto cur_mesh = obj->mesh;
    m_obj_config.material_id = obj->material->gpu_idx();

    // Legacy mode: instance_base = 0, firstInstance = slot
    // Shader: get_object_index() = slots[instance_base + gl_InstanceIndex]
    //       = slots[0 + slot] = slot (from identity buffer)
    m_obj_config.instance_base = 0;

    // Copy bindless texture indices from material
    copy_texture_indices(m_obj_config, obj->material);

    constexpr auto range = sizeof(gpu::push_constants_main);
    vkCmdPushConstants(cmd,
                       pctx.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       range,
                       &m_obj_config);

    // Draw using the object's slot via firstInstance
    if (!obj->mesh->has_indices())
    {
        vkCmdDraw(cmd, cur_mesh->vertices_size(), 1, 0, obj->slot());
    }
    else
    {
        vkCmdDrawIndexed(cmd, cur_mesh->indices_size(), 1, 0, 0, obj->slot());
    }
}

void
vulkan_render::bind_mesh(VkCommandBuffer cmd, mesh_data* cur_mesh)
{
    KRG_check(cur_mesh, "Should not be null");
    KRG_check(cur_mesh->m_vertex_buffer.buffer(), "Vertex buffer is VK_NULL_HANDLE");

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &cur_mesh->m_vertex_buffer.buffer(), &offset);

    if (cur_mesh->has_indices())
    {
        KRG_check(cur_mesh->m_index_buffer.buffer(), "Index buffer is VK_NULL_HANDLE");
        vkCmdBindIndexBuffer(cmd, cur_mesh->m_index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void
vulkan_render::bind_global_descriptors(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    // All main-pass shader effects share a compatible pipeline layout (identical
    // descriptor set layouts via the shared binding table + identical push-constant
    // ranges, since every shader includes push_constants_main in both stages). One
    // bind here keeps set 0 + set 1 in place across all material switches in the pass.
    if (m_main_global_set == VK_NULL_HANDLE || m_main_objects_set == VK_NULL_HANDLE)
    {
        return;
    }
    if (m_draw_batches.empty())
    {
        return;
    }

    auto layout = m_draw_batches[0].material->get_shader_effect()->m_pipeline_layout;
    VkDescriptorSet sets[2] = {m_main_global_set, m_main_objects_set};
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout,
                            KGPU_global_descriptor_sets,
                            2,
                            sets,
                            0,
                            nullptr);
    (void)current_frame;
}

void
vulkan_render::bind_material(VkCommandBuffer cmd,
                             material_data* cur_material,
                             render::frame_state& current_frame,
                             pipeline_ctx& ctx,
                             bool outline)
{
    auto pipeline = outline ? cur_material->get_shader_effect()->m_with_stencil_pipeline
                            : cur_material->get_shader_effect()->m_pipeline;
    ctx.pipeline_layout = cur_material->get_shader_effect()->m_pipeline_layout;
    ctx.cur_material_idx = cur_material->gpu_idx();
    ctx.cur_material_type_idx = cur_material->gpu_type_idx();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind texture set - either bindless global set or per-material set
    if (m_bindless_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                ctx.pipeline_layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &m_bindless_set,
                                0,
                                nullptr);
    }
    else if (cur_material->has_textures())
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                ctx.pipeline_layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &cur_material->get_textures_ds(),
                                0,
                                nullptr);
    }

    // Bind set 3 (material buffer) for this material's type segment.
    if (cur_material->has_gpu_data())
    {
        uint32_t type_idx = cur_material->gpu_type_idx();
        if (type_idx < m_material_sets.size() && m_material_sets[type_idx] != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    ctx.pipeline_layout,
                                    KGPU_materials_descriptor_sets,
                                    1,
                                    &m_material_sets[type_idx],
                                    0,
                                    nullptr);
        }
    }
    (void)current_frame;
}

void
vulkan_render::push_config(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t mat_id)
{
    m_obj_config.directional_light_id = 0U;
}

void
vulkan_render::schd_add_object(render::vulkan_render_data* obj_data)
{
    KRG_check(obj_data, "Should be always valid");
    m_object_bvh_dirty = true;

    if (obj_data->outlined)
    {
        // KRG_check(obj_data->queue_id != "transparent", "Not supported!");

        m_outline_render_object_queue[obj_data->queue_id].emplace_back(obj_data);
    }
    else if ((obj_data->layer_flags & render::LAYER_EDITOR_ONLY))
    {
        m_debug_render_object_queue[obj_data->queue_id].emplace_back(obj_data);
    }
    else if (obj_data->queue_id == "transparent")
    {
        m_transparent_render_object_queue.emplace_back(obj_data);
    }
    else
    {
        m_default_render_object_queue[obj_data->queue_id].emplace_back(obj_data);
    }

    for (auto& q : m_frames)
    {
        q.uploads.objects_queue.emplace_back(obj_data);
    }
}

void
vulkan_render::schd_update_object(render::vulkan_render_data* obj_data)
{
    KRG_check(obj_data, "Should be always valid");

    for (auto& q : m_frames)
    {
        q.uploads.objects_queue.emplace_back(obj_data);
    }
}

void
vulkan_render::schd_update_object_queue(render::vulkan_render_data* obj_data)
{
    schd_remove_object(obj_data);
    schd_add_object(obj_data);
}

void
vulkan_render::schd_remove_object(render::vulkan_render_data* obj_data)
{
    KRG_check(obj_data, "Should be always valid");
    m_object_bvh_dirty = true;

    {
        // KRG_check(obj_data->queue_id != "transparent", "Not supported!");

        const std::string id = obj_data->queue_id;

        auto& bucket = m_outline_render_object_queue[id];

        auto itr = bucket.find(obj_data);
        if (itr != bucket.end())
        {
            bucket.swap_and_remove(itr);

            if (bucket.get_size() == 0)
            {
                ALOG_TRACE("Dropping old queue");
                m_outline_render_object_queue.erase(id);
            }
        }
    }

    if ((obj_data->layer_flags & render::LAYER_EDITOR_ONLY))
    {
        const std::string id = obj_data->queue_id;

        auto it = m_debug_render_object_queue.find(id);
        if (it != m_debug_render_object_queue.end())
        {
            auto itr = it->second.find(obj_data);
            if (itr != it->second.end())
            {
                it->second.swap_and_remove(itr);

                if (it->second.get_size() == 0)
                {
                    m_debug_render_object_queue.erase(id);
                }
            }
        }
    }
    else if (obj_data->queue_id == "transparent")
    {
        auto itr = m_transparent_render_object_queue.find(obj_data);

        m_transparent_render_object_queue.swap_and_remove(itr);
    }
    else
    {
        const std::string id = obj_data->queue_id;

        auto& bucket = m_default_render_object_queue[id];

        auto itr = bucket.find(obj_data);
        if (itr != bucket.end())
        {
            bucket.swap_and_remove(itr);

            if (bucket.get_size() == 0)
            {
                ALOG_TRACE("Dropping old queue");
                m_default_render_object_queue.erase(id);
            }
        }
    }
}

void
vulkan_render::schd_add_material(render::material_data* mat_data)
{
    auto& mat_id = mat_data->get_type_id();

    auto segment = m_materials_layout.find(mat_id);

    const uint32_t INITIAL_MATERIAL_SEGMENT_RANGE_SIZE = 1024;

    if (!segment)
    {
        segment = m_materials_layout.add(
            mat_id, mat_data->get_gpu_data().size(), INITIAL_MATERIAL_SEGMENT_RANGE_SIZE);

        for (auto& q : m_frames)
        {
            while (segment->index >= q.uploads.materials_queue_set.get_size())
            {
                q.uploads.materials_queue_set.emplace_back();
            }
        }
    }
    mat_data->set_indexes(segment->alloc_id(), segment->index);

    for (auto& q : m_frames)
    {
        q.uploads.materials_queue_set[mat_data->gpu_type_idx()].emplace_back(mat_data);
        q.uploads.has_pending_materials = true;
    }
}

void
vulkan_render::schd_remove_material(render::material_data* mat_data)
{
    auto& mat_id = mat_data->get_type_id();
    auto segment = m_materials_layout.find(mat_id);

    if (segment)
    {
        segment->release_id(mat_data->gpu_idx());
        mat_data->invalidate_gpu_indexes();
    }
}

void
vulkan_render::update_transparent_objects_queue()
{
    for (auto& obj : m_transparent_render_object_queue)
    {
        obj->distance_to_camera = glm::length(obj->gpu_data.obj_pos - m_camera_data.position);
    }

    std::sort(m_transparent_render_object_queue.begin(),
              m_transparent_render_object_queue.end(),
              [](render::vulkan_render_data* l, render::vulkan_render_data* r)
              { return l->distance_to_camera > r->distance_to_camera; });
}

void
vulkan_render::prepare_ui_resources()
{
    ImGuiIO& io = ImGui::GetIO();

    // Create font texture
    auto font_rp =
        glob::glob_state().getr_vfs().real_path(vfs::rid("data://fonts/Roboto-Medium.ttf"));
    auto f = APATH(font_rp.value()).str();

    auto f_normal = io.Fonts->AddFontFromFileTTF(f.c_str(), 28.0f);
    auto f_big = io.Fonts->AddFontFromFileTTF(f.c_str(), 33.0f);

    glob::glob_state().getr_vulkan_render_loader().create_font(AID("normal"), f_normal);
    glob::glob_state().getr_vulkan_render_loader().create_font(AID("big"), f_big);

    int tex_width = 0, tex_height = 0;
    unsigned char* font_data = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

    auto size = tex_width * tex_height * 4 * sizeof(char);

    kryga::utils::buffer image_raw_buffer;
    image_raw_buffer.resize(size);
    memcpy(image_raw_buffer.data(), font_data, size);

    m_ui_txt = glob::glob_state().getr_vulkan_render_loader().create_texture(
        AID("font"), image_raw_buffer, tex_width, tex_height);

    auto ui_pass = glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("ui"));
    m_ui_target_txt = glob::glob_state().getr_vulkan_render_loader().create_texture(
        AID("ui_copy_txt"), ui_pass->get_color_images()[0], ui_pass->get_color_image_views()[0]);
}

void
vulkan_render::prepare_ui_pipeline()
{
    vfs::rid se_ui_base("data://packages/base.apkg/class/shader_effects/ui");

    {
        kryga::utils::buffer vert, frag;

        vfs::load_buffer(se_ui_base / "se_uioverlay.vert", vert);
        vfs::load_buffer(se_ui_base / "se_uioverlay.frag", frag);

        auto layout = render::gpu_dynobj_builder()
                          .set_id(AID("interface"))
                          .add_field(AID("in_pos"), kryga::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_UV"), kryga::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_color"), kryga::render::gpu_type::g_color, 1)
                          .finalize();

        auto ui_pass = glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("ui"));

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = true;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.expected_input_vertex_layout = std::move(layout);

        ui_pass->create_shader_effect(AID("se_ui"), se_ci, m_ui_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_txt;
        samples.front().slot = 0;

        m_ui_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
            AID("mat_ui"), AID("ui"), samples, *m_ui_se, utils::dynobj{});
    }
    {
        kryga::utils::buffer vert, frag;

        vfs::load_buffer(se_ui_base / "se_upload.vert", vert);
        vfs::load_buffer(se_ui_base / "se_upload.frag", frag);

        auto main_pass =
            glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("main"));

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;

        main_pass->create_shader_effect(AID("se_ui_copy"), se_ci, m_ui_copy_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_target_txt;
        samples.front().slot = 0;

        m_ui_target_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
            AID("mat_ui_copy"), AID("ui_copy"), samples, *m_ui_copy_se, utils::dynobj{});
    }
}

void
vulkan_render::update_ui(frame_state& fs)
{
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    auto device = glob::glob_state().get_render_device();
    ImDrawData* im_draw_data = ImGui::GetDrawData();

    if (!im_draw_data)
    {
        return;
    };

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertex_buffer_size = im_draw_data->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize index_buffer_size = im_draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    if ((vertex_buffer_size == 0) || (index_buffer_size == 0))
    {
        return;
    }

    // Regrow buffers when needed (with 2x headroom to avoid per-frame reallocation)
    if (vertex_buffer_size > fs.ui.vertex_buffer.get_alloc_size() ||
        index_buffer_size > fs.ui.index_buffer.get_alloc_size())
    {
        VkBufferCreateInfo staging_buffer_ci = {};
        staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_ci.pNext = nullptr;

        VmaAllocationCreateInfo vma_ci = {};
        vma_ci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        staging_buffer_ci.size = vertex_buffer_size * 2;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        fs.ui.vertex_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_ci);

        staging_buffer_ci.size = index_buffer_size * 2;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        fs.ui.index_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_ci);
    }

    fs.ui.vertex_count = im_draw_data->TotalVtxCount;
    fs.ui.index_count = im_draw_data->TotalIdxCount;

    // Upload data
    fs.ui.vertex_buffer.begin();
    fs.ui.index_buffer.begin();

    auto vtx_dst = (ImDrawVert*)fs.ui.vertex_buffer.allocate_data((uint32_t)vertex_buffer_size);
    auto idx_dst = (ImDrawIdx*)fs.ui.index_buffer.allocate_data((uint32_t)index_buffer_size);

    for (int n = 0; n < im_draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = im_draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    fs.ui.vertex_buffer.end();
    fs.ui.index_buffer.end();

    fs.ui.vertex_buffer.flush();
    fs.ui.index_buffer.flush();
}

void
vulkan_render::draw_ui(frame_state& fs)
{
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    auto im_draw_data = ImGui::GetDrawData();

    if ((!im_draw_data) || (im_draw_data->CmdListsCount == 0))
    {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();

    VkViewport viewport{};
    viewport.width = io.DisplaySize.x;
    viewport.height = io.DisplaySize.y;
    viewport.minDepth = 0.;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.extent.width = (uint32_t)io.DisplaySize.x;
    scissor.extent.height = (uint32_t)io.DisplaySize.y;
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    auto cmd = fs.frame->m_main_command_buffer;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_se->m_pipeline);

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_ui_se->m_pipeline_layout,
                            0,
                            1,
                            &m_ui_mat->get_textures_ds(),
                            0,
                            nullptr);

    m_ui_push_constants.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    m_ui_push_constants.translate = glm::vec2(-1.0f);

    vkCmdPushConstants(cmd,
                       m_ui_se->m_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(ui_push_constants),
                       &m_ui_push_constants);

    KRG_check(fs.ui.vertex_buffer.buffer(), "UI vertex buffer must be valid");
    KRG_check(fs.ui.index_buffer.buffer(), "UI index buffer must be valid");

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &fs.ui.vertex_buffer.buffer(), offsets);
    vkCmdBindIndexBuffer(cmd, fs.ui.index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT16);

    int32_t vertex_offset = 0;
    int32_t index_offset = 0;
    for (int32_t i = 0; i < im_draw_data->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = im_draw_data->CmdLists[i];
        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(fs.frame->m_main_command_buffer, 0, 1, &scissorRect);
            vkCmdDrawIndexed(fs.frame->m_main_command_buffer,
                             pcmd->ElemCount,
                             1,
                             index_offset,
                             vertex_offset,
                             0);
            index_offset += pcmd->ElemCount;
        }
        vertex_offset += cmd_list->VtxBuffer.Size;
    }
}

}  // namespace render
}  // namespace kryga

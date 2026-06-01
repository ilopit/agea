#include "vulkan_render/kryga_render.h"

#include <tracy/Tracy.hpp>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/render_system.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_render_data.h"

#include <gpu_types/gpu_generic_constants.h>

#include <utils/kryga_log.h>

#include <kryga_port/imgui.h>

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

        batch.material->get_shader_effect()->push_constants(cmd, &m_obj_config);

        draw_mesh(cmd, batch.mesh, batch.instance_count);
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

            obj->material->get_shader_effect()->push_constants(cmd, &m_obj_config);

            draw_mesh(cmd, obj->mesh);

            ++transparent_idx;
        }
    }

    // Debug overlays + outline post are drawn at full res. When render-scale is
    // on they happen in the composite pass instead — drawing them here would put
    // them on the lowres scene target, getting smeared by the upscale.
    if (!m_render_config.render_scale.enabled)
    {
        draw_debug_overlay(cmd, current_frame);
        draw_outline_post(cmd, current_frame);
        draw_ui_overlay(cmd, current_frame);
    }
}

// ============================================================================
// Draw Functions - Grid (shared)
// ============================================================================

void
vulkan_render::draw_grid(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    if (!m_render_config.debug.show_grid || !m_grid_mat || !m_grid_se)
    {
        return;
    }

    // Bindless set must be (re)bound under grid's own pipeline layout — push
    // constant ranges differ from previously bound pipelines (e.g. debug_wire),
    // which under Vulkan layout-compatibility rules invalidates the prior bind.
    draw_fullscreen_quad(cmd, m_grid_se, &m_grid_pc);
}

// ============================================================================
// Draw Functions - Debug overlay (full-res, always drawn after the scene)
// ============================================================================

void
vulkan_render::draw_debug_overlay(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    // Build-time gate: debug overlays (light gizmo billboards, grid, debug
    // wireframes) are editor-only and have no place in a shipped game build.
    // The render lib is compiled twice — once per flavor — so this whole
    // function is dead code in vulkan_render_impl_game.
    //
    // The runtime `debug.editor_mode` flag below is the editor's "preview as
    // game" toggle: turn it off to see the scene as the game build will see it
    // without restarting. It defaults to true and is shared via rtcache, so a
    // pure runtime check would let an editor session leak `editor_mode=true`
    // into the next game launch — which is exactly the bug this guard fixes.
#if !KRG_EDITOR
    return;
#else
    if (!m_render_config.debug.editor_mode)
    {
        return;
    }
#endif

    // Editor-only render bucket (LAYER_EDITOR_ONLY objects, e.g. light gizmo billboards).
    // Drawn unlit — lighting state saved/restored around the loop.
    if (!m_debug_draw_batches.empty())
    {
        uint32_t saved_dir = m_obj_config.enable_directional_light;
        uint32_t saved_local = m_obj_config.enable_local_lights;
        uint32_t saved_baked = m_obj_config.enable_baked_light;

        m_obj_config.enable_directional_light = 0;
        m_obj_config.enable_local_lights = 0;
        m_obj_config.enable_baked_light = 0;

        pipeline_ctx pctx{};
        material_data* cur_material = nullptr;

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

            batch.material->get_shader_effect()->push_constants(cmd, &m_obj_config);

            draw_mesh(cmd, batch.mesh, batch.instance_count);
        }

        m_obj_config.enable_directional_light = saved_dir;
        m_obj_config.enable_local_lights = saved_local;
        m_obj_config.enable_baked_light = saved_baked;
    }

    draw_debug_lights(cmd, current_frame);
    draw_grid(cmd, current_frame);
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

        batch.material->get_shader_effect()->push_constants(cmd, &m_obj_config);

        draw_mesh(cmd, batch.mesh, batch.instance_count);
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

    struct outline_pc
    {
        glm::vec4 outline_color;
        glm::vec2 texel_size;
        float thickness;
        uint32_t mask_texture_idx;
    };

    outline_pc pc;
    pc.outline_color = glm::vec4(0.2f, 0.9f, 0.4f, 1.0f);  // green
    pc.texel_size = glm::vec2(1.0f / m_width, 1.0f / m_height);
    pc.thickness = 1.5f;
    pc.mask_texture_idx = m_selection_mask_bindless_idx;

    draw_fullscreen_quad(cmd, m_outline_post_se, &pc);
}

// ============================================================================
// Draw Functions - Scene Upscale (render_scale)
// ============================================================================

void
vulkan_render::draw_scene_upscale(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    if (!m_scene_upscale_mat || !m_scene_upscale_se)
    {
        return;
    }

    copy_texture_indices(m_obj_config, m_scene_upscale_mat);
    draw_fullscreen_quad(cmd, m_scene_upscale_se, &m_obj_config);
}

void
vulkan_render::draw_composite(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    draw_scene_upscale(cmd, current_frame);
    draw_depth_outline(cmd, current_frame);
    // Full-res overlays — order: debug visuals, outline edge-detect, UI on top.
    draw_debug_overlay(cmd, current_frame);
    draw_outline_post(cmd, current_frame);
    draw_ui_overlay(cmd, current_frame);
}

void
vulkan_render::draw_depth_outline(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    if (!m_render_config.outline.enabled || !m_depth_outline_se ||
        m_scene_depth_bindless_idx == 0xFFFFFFFFu)
    {
        return;
    }

    struct outline_pc
    {
        glm::vec4 outline_color;
        glm::vec2 texel_size;
        float depth_threshold;
        float normal_threshold;
        uint32_t depth_texture_idx;
        float near_plane;
        float far_plane;
        uint32_t _pad;
    };

    outline_pc pc;
    const auto& cfg = m_render_config.outline;
    pc.outline_color = glm::vec4(cfg.color[0], cfg.color[1], cfg.color[2], cfg.color[3]);
    // texel_size is in low-res scene coordinates — that's where the depth texture lives
    pc.texel_size = glm::vec2(1.0f / m_scene_lowres_width, 1.0f / m_scene_lowres_height);
    pc.depth_threshold = cfg.depth_threshold;
    pc.normal_threshold = cfg.normal_threshold;
    pc.depth_texture_idx = m_scene_depth_bindless_idx;
    pc.near_plane = m_cluster_config.near_plane;
    pc.far_plane = m_cluster_config.far_plane;
    pc._pad = 0;

    draw_fullscreen_quad(cmd, m_depth_outline_se, &pc);
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

    auto m = glob::glob_state().getr_render().loader.get_mesh_data(AID("plane_mesh"));
    if (!m)
    {
        return;
    }

    // UI overlay shader has a different (incompatible) pipeline layout for sets 0-1
    // It only uses set 2 (textures) and push constants, so we bind those directly
    auto pipeline_layout = m_ui_target_mat->get_shader_effect()->m_pipeline_layout;

    vkCmdBindPipeline(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_target_mat->get_shader_effect()->m_pipeline);

    // Rebind the global bindless set with UI's compatible pipeline layout.
    KRG_check(m_bindless_set != VK_NULL_HANDLE, "bindless set must exist");
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout,
                            KGPU_textures_descriptor_sets,
                            1,
                            &m_bindless_set,
                            0,
                            nullptr);

    bind_mesh(cmd, m);

    // Push constants with texture indices for UI material
    copy_texture_indices(m_obj_config, m_ui_target_mat);
    m_ui_target_mat->get_shader_effect()->push_constants(cmd, &m_obj_config);

    draw_mesh(cmd, m);
}

// ============================================================================
// Draw Functions - ImGui
// ============================================================================

#if KRG_HAS_IMGUI
void
vulkan_render::draw_ui(frame_state& fs)
{
    // Read this frame's snapshot slot, NOT the live ImGui draw data (the main
    // thread may be mid-NewFrame on ImGui's single buffer). update_ui filled the
    // GPU buffers from this same slot.
    const ui_draw_snapshot& s = m_ui_snapshots[m_draw_frame_slot];

    if (!s.valid || s.cmds.empty() || s.total_vtx == 0 || s.total_idx == 0)
    {
        return;
    }

    if (!fs.ui.vertex_buffer.buffer() || !fs.ui.index_buffer.buffer())
    {
        return;
    }

    // Viewport and scissor are in swapchain image pixel coords.
    auto& device = glob::glob_state().getr_render().device;
    auto extent = device.swapchain_extent();
    VkViewport viewport{};
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.extent = extent;
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    auto cmd = fs.frame->m_main_command_buffer;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_se->m_pipeline);

    // Bind the global bindless set; the font is sampled by index (no per-material
    // descriptor set). Mirrors the bindless main/UI-copy draws.
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_ui_se->m_pipeline_layout,
                            KGPU_textures_descriptor_sets,
                            1,
                            &m_bindless_set,
                            0,
                            nullptr);

    m_ui_push_constants.scale = glm::vec2(2.0f / s.display_size[0], 2.0f / s.display_size[1]);
    m_ui_push_constants.translate = glm::vec2(-1.0f);
    m_ui_push_constants.tex_index = m_ui_txt->get_bindless_index();
    m_ui_push_constants.sampler_index = KGPU_SAMPLER_LINEAR_CLAMP;

    vkCmdPushConstants(cmd,
                       m_ui_se->m_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(ui_push_constants),
                       &m_ui_push_constants);

    // Defensive: `update_ui` is the producer for these buffers and uses a
    // near-identical early-exit condition, but the two early-exits aren't
    // byte-identical (update_ui checks size==0, draw_ui checks count==0 plus
    // CmdListsCount). If a frame slips through with valid ImGui draw data
    // but no buffer yet (first-frame-after-init race or similar), skip this
    // frame's UI rather than dereferencing a null VkBuffer.
    if (!fs.ui.vertex_buffer.buffer() || !fs.ui.index_buffer.buffer())
    {
        return;
    }

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &fs.ui.vertex_buffer.buffer(), offsets);
    vkCmdBindIndexBuffer(cmd, fs.ui.index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT16);

    for (const ui_draw_cmd& dc : s.cmds)
    {
        VkRect2D scissorRect;
        scissorRect.offset.x = std::max((int32_t)dc.clip[0], 0);
        scissorRect.offset.y = std::max((int32_t)dc.clip[1], 0);
        scissorRect.extent.width = (uint32_t)(dc.clip[2] - dc.clip[0]);
        scissorRect.extent.height = (uint32_t)(dc.clip[3] - dc.clip[1]);
        vkCmdSetScissor(fs.frame->m_main_command_buffer, 0, 1, &scissorRect);
        vkCmdDrawIndexed(
            fs.frame->m_main_command_buffer, dc.elem_count, 1, dc.idx_offset, dc.vtx_offset, 0);
    }
}
#endif  // KRG_HAS_IMGUI

}  // namespace render
}  // namespace kryga

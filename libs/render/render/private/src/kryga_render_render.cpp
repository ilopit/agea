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

#include <kryga_port/imgui.h>

#include <vfs/vfs.h>
#include <vfs/io.h>
#include <global_state/global_state.h>
#include <vulkan_render/render_system.h>

#include <shader_system/shader_loader.h>

#include <algorithm>

namespace kryga
{
namespace render
{

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
        frame.buffers.instance_slots = glob::glob_state().getr_render().device.create_buffer(
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
            !m_frustum.is_sphere_visible(obj->gpu_data.bounding_sphere_center,
                                         obj->gpu_data.bounding_radius))
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
vulkan_render::draw_mesh(VkCommandBuffer cmd,
                         mesh_data* m,
                         uint32_t instance_count,
                         uint32_t first_instance)
{
    if (m->has_indices())
    {
        vkCmdDrawIndexed(cmd, m->indices_size(), instance_count, 0, 0, first_instance);
    }
    else
    {
        vkCmdDraw(cmd, m->vertices_size(), instance_count, 0, first_instance);
    }
}

void
vulkan_render::bind_bindless(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    if (m_bindless_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &m_bindless_set,
                                0,
                                nullptr);
    }
}

void
vulkan_render::draw_fullscreen_quad(VkCommandBuffer cmd,
                                    shader_effect_data* se,
                                    const void* push_data)
{
    auto m = glob::glob_state().getr_render().loader.get_mesh_data(AID("plane_mesh"));
    if (!m)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);
    bind_bindless(cmd, se->m_pipeline_layout);
    bind_mesh(cmd, m);
    se->push_constants(cmd, push_data);
    draw_mesh(cmd, m);
}

void
vulkan_render::bind_global_descriptors(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    // Sets 0/1 removed — buffers are now accessed via BDA pointer table in push constants.
    // This function is kept as a no-op to avoid touching all call sites.
    // TODO: remove call sites in Phase 5 cleanup.
    (void)cmd;
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

    // Material data — compute BDA address for this material type's segment
    if (cur_material->has_gpu_data())
    {
        auto& sm = m_materials_layout.at(cur_material->gpu_type_idx());
        m_obj_config.bdaf_material =
            gpu::make_bda_addr(current_frame.buffers.materials.device_address() + sm.offset);
        m_bda_material_bound = true;
    }

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

// ============================================================================
// ImGui-using methods — bodies compiled out in game builds. Call sites are
// also guarded with #if KRG_EDITOR (init.cpp / frame.cpp / passes.cpp), so
// linker never resolves these symbols in kryga_game.
// ============================================================================
#if KRG_EDITOR
void
vulkan_render::prepare_ui_resources()
{
    ImGuiIO& io = ImGui::GetIO();

    // Create font texture. Read TTF bytes via VFS so APK-asset mounts work
    // (Android has no filesystem path for bundled fonts).
    // Note: AddFontFromMemoryTTF takes ownership of the pointer and frees
    // it on atlas destruction — give each call a heap copy.
    std::vector<uint8_t> font_bytes;
    if (!glob::glob_state().getr_vfs().read_bytes(vfs::rid("data://fonts/Roboto-Medium.ttf"),
                                                  font_bytes))
    {
        KRG_never("Failed to load font data://fonts/Roboto-Medium.ttf");
    }

    auto clone_ttf = [&font_bytes]() -> void*
    {
        void* buf = IM_ALLOC(font_bytes.size());
        memcpy(buf, font_bytes.data(), font_bytes.size());
        return buf;
    };

    auto f_normal =
        io.Fonts->AddFontFromMemoryTTF(clone_ttf(), static_cast<int>(font_bytes.size()), 28.0f);
    auto f_big =
        io.Fonts->AddFontFromMemoryTTF(clone_ttf(), static_cast<int>(font_bytes.size()), 33.0f);

    glob::glob_state().getr_render().loader.create_font(AID("normal"), f_normal);
    glob::glob_state().getr_render().loader.create_font(AID("big"), f_big);

    int tex_width = 0, tex_height = 0;
    unsigned char* font_data = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

    auto size = tex_width * tex_height * 4 * sizeof(char);

    kryga::utils::buffer image_raw_buffer;
    image_raw_buffer.resize(size);
    memcpy(image_raw_buffer.data(), font_data, size);

    m_ui_txt = glob::glob_state().getr_render().loader.create_texture(
        AID("font"), image_raw_buffer, tex_width, tex_height);

    auto ui_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("ui"));
    m_ui_target_txt = glob::glob_state().getr_render().loader.create_texture(
        AID("ui_copy_txt"), ui_pass->get_color_images()[0], ui_pass->get_color_image_views()[0]);
}

void
vulkan_render::prepare_ui_pipeline()
{
    vfs::rid se_ui_base("data://packages/base.apkg/class/shader_effects/ui");

    {
        auto vert_r = render::shader_loader::load(se_ui_base / "se_uioverlay.vert.spv");
        auto frag_r = render::shader_loader::load(se_ui_base / "se_uioverlay.frag.spv");
        if (!vert_r || !frag_r)
        {
            ALOG_ERROR("Failed to load se_uioverlay shaders — UI pipeline not created");
            return;
        }
        auto& vert = *vert_r;
        auto& frag = *frag_r;

        auto layout = render::gpu_dynobj_builder()
                          .set_id(AID("interface"))
                          .add_field(AID("in_pos"), kryga::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_UV"), kryga::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_color"), kryga::render::gpu_type::g_color, 1)
                          .finalize();

        auto ui_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("ui"));

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

        m_ui_mat = glob::glob_state().getr_render().loader.create_material(
            AID("mat_ui"), AID("ui"), samples, *m_ui_se, utils::dynobj{});
    }
    {
        auto vert_r = render::shader_loader::load(se_ui_base / "se_upload.vert.spv");
        auto frag_r = render::shader_loader::load(se_ui_base / "se_upload.frag.spv");
        if (!vert_r || !frag_r)
        {
            ALOG_ERROR("Failed to load se_upload shaders — UI copy pipeline not created");
            return;
        }
        auto& vert = *vert_r;
        auto& frag = *frag_r;

        // In render_scale mode the UI copy is drawn inside the composite pass, which has
        // a different depth format than main. Create the pipeline against the pass
        // that will actually execute it.
        auto host_pass =
            glob::glob_state().getr_render().loader.get_render_pass(get_host_pass_id());

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.depth_write = false;

        host_pass->create_shader_effect(AID("se_ui_copy"), se_ci, m_ui_copy_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_target_txt;
        samples.front().slot = 0;

        m_ui_target_mat = glob::glob_state().getr_render().loader.create_material(
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

    auto& device = glob::glob_state().getr_render().device;
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
#endif  // KRG_EDITOR

// ============================================================================
// Scene upscale pipeline — runs in BOTH editor and game when render_scale is on.
// Reuses se_upload from the ui/ shaders (a texture blit) but is otherwise
// independent of ImGui/UI, so cannot live inside the editor-gated UI block.
// ============================================================================
void
vulkan_render::prepare_scene_upscale_pipeline()
{
    if (!m_render_config.render_scale.enabled || m_scene_lowres_views.empty())
    {
        return;
    }

    vfs::rid se_ui_base("data://packages/base.apkg/class/shader_effects/ui");

    auto vert_r = render::shader_loader::load(se_ui_base / "se_upload.vert.spv");
    auto frag_r = render::shader_loader::load(se_ui_base / "se_upload.frag.spv");
    if (!vert_r || !frag_r)
    {
        ALOG_ERROR("Failed to load se_upload shaders — scene upscale pipeline not created");
        return;
    }

    auto composite_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("composite"));
    KRG_check(composite_pass, "composite pass must exist when render_scale is enabled");

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &(*vert_r);
    se_ci.frag_buffer = &(*frag_r);
    se_ci.is_wire = false;
    se_ci.enable_dynamic_state = false;
    se_ci.alpha = alpha_mode::ui;
    se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
    se_ci.depth_write = false;

    composite_pass->create_shader_effect(AID("se_scene_upscale"), se_ci, m_scene_upscale_se);

    m_scene_upscale_txt = glob::glob_state().getr_render().loader.create_texture(
        AID("scene_lowres_txt"), m_scene_lowres_images[0], m_scene_lowres_views[0]);

    std::vector<texture_sampler_data> samples(1);
    samples.front().texture = m_scene_upscale_txt;
    samples.front().slot = 0;

    m_scene_upscale_mat =
        glob::glob_state().getr_render().loader.create_material(AID("mat_scene_upscale"),
                                                                AID("scene_upscale"),
                                                                samples,
                                                                *m_scene_upscale_se,
                                                                utils::dynobj{});

    // Override the default (LINEAR_REPEAT) sampler with NEAREST_CLAMP so the
    // upscale keeps chunky pixel edges instead of bilinear-smoothing them.
    if (m_scene_upscale_mat)
    {
        m_scene_upscale_mat->set_bindless_sampler_index(0, KGPU_SAMPLER_NEAREST_CLAMP);
    }
}

}  // namespace render
}  // namespace kryga

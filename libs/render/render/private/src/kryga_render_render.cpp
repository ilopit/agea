#include "vulkan_render/kryga_render.h"
#include "vulkan_render/render_thread.h"

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
    KRG_check_render_thread();
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

namespace
{
// Per-pass shadow cull volume. One value type covers all three pass kinds
// (cascade/spot share a frustum; point splits into front/back hemispheres) so
// build_culled_shadow_batches needs no template and the call sites need no
// lambdas. Construct via the from_frustum/point factories.
struct shadow_cull_volume
{
    enum class kind
    {
        frustum,      // cascade ortho box or spot perspective frustum
        point_front,  // point light, front paraboloid hemisphere
        point_back    // point light, back paraboloid hemisphere
    };

    kind which = kind::frustum;
    frustum frust;                          // kind::frustum
    glm::vec3 position{0.0f};               // kind::point_*
    glm::vec3 front_dir{0.0f, 0.0f, 1.0f};  // kind::point_*
    float light_radius = 0.0f;              // kind::point_* (light reach)

    bool
    is_visible(const glm::vec3& center, float radius) const
    {
        if (which == kind::frustum)
        {
            return frust.is_sphere_visible(center, radius);
        }
        // Point hemisphere: reject outside the light sphere, then split by the
        // sign of the projection onto front_dir (front keeps >= -reach, back <= reach).
        const float reach = light_radius + radius;
        if (glm::distance(center, position) > reach)
        {
            return false;
        }
        const float d = glm::dot(center - position, front_dir);
        return which == kind::point_back ? (d <= reach) : (d >= -reach);
    }

    static shadow_cull_volume
    from_frustum(const glm::mat4& view_proj)
    {
        shadow_cull_volume v;
        v.which = kind::frustum;
        v.frust.extract_planes(view_proj);
        return v;
    }

    static shadow_cull_volume
    point(const glm::vec3& position, const glm::vec3& front_dir, float light_radius, bool back_face)
    {
        shadow_cull_volume v;
        v.which = back_face ? kind::point_back : kind::point_front;
        v.position = position;
        v.front_dir = front_dir;
        v.light_radius = light_radius;
        return v;
    }
};

// Build shadow-caster batches for one queue, keeping only objects whose bounding
// sphere passes `cull` (the per-pass light volume). Mirrors the mesh-grouping of
// build_batches_for_queue_into but: shadow casters only, no stats/outline, and it
// appends visible slots compactly so each pass gets its own contiguous ranges.
void
build_culled_shadow_batches(render_line_container& r,
                            std::vector<uint32_t>& staging,
                            std::vector<draw_batch>& out_batches,
                            const shadow_cull_volume& cull)
{
    if (r.empty())
    {
        return;
    }

    mesh_data* cur_mesh = nullptr;
    auto batch_start = (uint32_t)staging.size();

    auto flush = [&]()
    {
        uint32_t instance_count = (uint32_t)staging.size() - batch_start;
        if (cur_mesh && instance_count > 0)
        {
            out_batches.push_back({.mesh = cur_mesh,
                                   .material = r.front()->material,
                                   .instance_count = instance_count,
                                   .first_instance_offset = batch_start,
                                   .outlined = false,
                                   .cast_shadows = true});
        }
        batch_start = (uint32_t)staging.size();
    };

    for (auto& obj : r)
    {
        if (!(obj->layer_flags & render::LAYER_CAST_SHADOWS))
        {
            continue;
        }
        if (!cull.is_visible(obj->gpu_data.bounding_sphere_center, obj->gpu_data.bounding_radius))
        {
            continue;
        }

        if (cur_mesh && cur_mesh != obj->mesh)
        {
            flush();
        }
        cur_mesh = obj->mesh;
        staging.push_back(obj->slot());
    }
    flush();
}

// Build one shadow pass's batches from the default + outline caster queues,
// appending visible slots into the shared staging buffer. Replaces the former
// `build_all` lambda. `out` is cleared first (per-pass persistent batch list).
void
build_shadow_pass_batches(std::unordered_map<std::string, render_line_container>& default_queue,
                          std::unordered_map<std::string, render_line_container>& outline_queue,
                          std::vector<uint32_t>& staging,
                          std::vector<draw_batch>& out,
                          const shadow_cull_volume& cull)
{
    out.clear();
    for (auto& [queue_id, container] : default_queue)
    {
        build_culled_shadow_batches(container, staging, out, cull);
    }
    for (auto& [queue_id, container] : outline_queue)
    {
        build_culled_shadow_batches(container, staging, out, cull);
    }
}
}  // namespace

void
vulkan_render::build_batches_for_queue(render_line_container& r, bool outlined)
{
    build_batches_for_queue_into(r, outlined, m_draw_batches, true);
}

void
vulkan_render::build_batches_for_queue_into(render_line_container& r,
                                            bool outlined,
                                            std::vector<draw_batch>& out_batches,
                                            bool apply_frustum_cull)
{
    if (r.empty())
    {
        return;
    }

    mesh_data* cur_mesh = nullptr;
    bool cur_cast_shadows = true;
    auto batch_start = (uint32_t)m_instance_slots_staging.size();

    for (auto& obj : r)
    {
        // Stats and camera-frustum culling only apply to the camera-visible pass.
        // Shadow caster batches (apply_frustum_cull == false) must NOT be culled
        // against the camera, or off-screen casters drop out of the shadow atlas.
        if (apply_frustum_cull)
        {
            ++m_all_draws;

            if (m_render_config.debug.frustum_culling &&
                !m_frustum.is_sphere_visible(obj->gpu_data.bounding_sphere_center,
                                             obj->gpu_data.bounding_radius))
            {
                ++m_culled_draws;
                continue;
            }
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
        build_batches_for_queue_into(container, false, m_debug_draw_batches, true);
    }

    // Shadow caster batches — culled per pass against each light's volume, NOT the
    // camera frustum, so off-screen casters still render into the shadow atlas (#1)
    // while casters outside a given light's reach are skipped (#7). Each pass
    // appends its own compact slot range into the shared staging buffer.
    // Requires compute_shadow_matrices()/select_shadowed_lights() to have run first
    // (called before prepare_instance_data in prepare_draw_resources).
    if (m_render_config.shadows.enabled)
    {
        // CSM cascades: cull against each cascade's ortho frustum.
        for (uint32_t c = 0; c < m_render_config.shadows.cascade_count; ++c)
        {
            build_shadow_pass_batches(m_default_render_object_queue,
                                      m_outline_render_object_queue,
                                      m_instance_slots_staging,
                                      m_cascade_shadow_batches[c],
                                      shadow_cull_volume::from_frustum(
                                          m_shadow_config.directional.cascades[c].view_proj));
        }

        // Local lights: spots cull against their perspective frustum; points use a
        // sphere test split by hemisphere (front/back paraboloid tiles).
        for (uint32_t i = 0; i < m_shadow_config.shadowed_local_count; ++i)
        {
            const auto& cull = m_local_shadow_cull[i];

            if (cull.type == KGPU_light_type_point)
            {
                build_shadow_pass_batches(
                    m_default_render_object_queue,
                    m_outline_render_object_queue,
                    m_instance_slots_staging,
                    m_local_shadow_batches[i * 2],
                    shadow_cull_volume::point(cull.position, cull.front_dir, cull.radius, false));
                build_shadow_pass_batches(
                    m_default_render_object_queue,
                    m_outline_render_object_queue,
                    m_instance_slots_staging,
                    m_local_shadow_batches[i * 2 + 1],
                    shadow_cull_volume::point(cull.position, cull.front_dir, cull.radius, true));
            }
            else
            {
                build_shadow_pass_batches(
                    m_default_render_object_queue,
                    m_outline_render_object_queue,
                    m_instance_slots_staging,
                    m_local_shadow_batches[i * 2],
                    shadow_cull_volume::from_frustum(m_shadow_config.local_shadows[i].view_proj));
            }
        }
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
    KRG_check(m_fullscreen_quad, "fullscreen quad missing — system resources not initialized");
    auto m = m_fullscreen_quad;

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

    // Bind the global bindless texture set (mandatory — Vulkan 1.2 + bindless).
    KRG_check(m_bindless_set != VK_NULL_HANDLE, "bindless set must exist");
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ctx.pipeline_layout,
                            KGPU_textures_descriptor_sets,
                            1,
                            &m_bindless_set,
                            0,
                            nullptr);
}

void
vulkan_render::push_config(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t mat_id)
{
    m_obj_config.directional_light_id = 0U;
}

void
vulkan_render::stage_add_object(render::vulkan_render_data* obj_data)
{
    KRG_check_render_thread();
    KRG_check(obj_data, "Should be always valid");
    KRG_check(!obj_data->is_pending_release(), "Adding a dead object");
    m_object_bvh_dirty = true;

    if (obj_data->layer_flags & render::LAYER_VISIBLE)
    {
        if (obj_data->outlined)
        {
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
    }

    for (auto& q : m_frames)
    {
        q.uploads.objects_queue.emplace_back(obj_data);
    }
}

void
vulkan_render::stage_update_object(render::vulkan_render_data* obj_data)
{
    KRG_check_render_thread();
    KRG_check(obj_data, "Should be always valid");
    KRG_check(!obj_data->is_pending_release(), "Updating a dead object");

    for (auto& q : m_frames)
    {
        q.uploads.objects_queue.emplace_back(obj_data);
    }
}

void
vulkan_render::stage_update_object_queue(render::vulkan_render_data* obj_data)
{
    KRG_check_render_thread();
    stage_remove_object(obj_data);
    stage_add_object(obj_data);
}

void
vulkan_render::stage_remove_object(render::vulkan_render_data* obj_data)
{
    KRG_check_render_thread();
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
vulkan_render::stage_add_material(render::material_data* mat_data)
{
    KRG_check_render_thread();
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
vulkan_render::stage_remove_material(render::material_data* mat_data)
{
    KRG_check_render_thread();
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
// ImGui-using methods — compiled in when KRG_HAS_IMGUI is set (both editor
// and non-shipping game builds). Call sites are similarly guarded.
// ============================================================================
#if KRG_HAS_IMGUI
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

    // The atlas owns the ImFont*s; the first added is ImGui's default. (The old
    // loader font cache was a write-only registry — nothing ever looked fonts up.)
    io.Fonts->AddFontFromMemoryTTF(clone_ttf(), static_cast<int>(font_bytes.size()), 28.0f);
    io.Fonts->AddFontFromMemoryTTF(clone_ttf(), static_cast<int>(font_bytes.size()), 33.0f);

    int tex_width = 0, tex_height = 0;
    unsigned char* font_data = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

    auto size = tex_width * tex_height * 4 * sizeof(char);

    kryga::utils::buffer image_raw_buffer;
    image_raw_buffer.resize(size);
    memcpy(image_raw_buffer.data(), font_data, size);

    m_ui_txt = create_texture(
        AID("font"), image_raw_buffer, tex_width, tex_height);

    auto ui_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("ui"));
    m_ui_target_txt = create_texture(
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

        m_ui_mat = create_material(
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

        m_ui_target_mat = create_material(
            AID("mat_ui_copy"), AID("ui_copy"), samples, *m_ui_copy_se, utils::dynobj{});
    }
}

void
vulkan_render::capture_ui_snapshot()
{
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    ImDrawData* dd = ImGui::GetDrawData();

    // Write into the main thread's current input slot (frame parity); the render
    // thread reads the matching slot via m_draw_frame_slot. The pipeline gate
    // keeps the render thread off this slot, so no publish/atomic is needed.
    ui_draw_snapshot& s = m_ui_snapshots[m_build_frame_slot];

    s.cmds.clear();
    s.vtx.clear();
    s.idx.clear();
    s.total_vtx = 0;
    s.total_idx = 0;
    s.valid = false;

    if (!dd || dd->CmdListsCount == 0 || dd->TotalVtxCount == 0 || dd->TotalIdxCount == 0)
    {
        // Leave this slot invalid so the render thread draws no UI this frame
        // rather than reusing a stale snapshot.
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    s.display_size[0] = io.DisplaySize.x;
    s.display_size[1] = io.DisplaySize.y;

    s.vtx.resize(static_cast<size_t>(dd->TotalVtxCount) * sizeof(ImDrawVert));
    s.idx.resize(static_cast<size_t>(dd->TotalIdxCount) * sizeof(ImDrawIdx));

    auto* vtx_dst = reinterpret_cast<ImDrawVert*>(s.vtx.data());
    auto* idx_dst = reinterpret_cast<ImDrawIdx*>(s.idx.data());

    // Offsets mirror update_ui/draw_ui's old logic exactly: vertices/indices are
    // concatenated per cmd-list into one buffer; idx_offset runs across all
    // commands by ElemCount, vtx_offset is the per-cmd-list base.
    int32_t vtx_base = 0;
    uint32_t idx_run = 0;
    for (int n = 0; n < dd->CmdListsCount; ++n)
    {
        const ImDrawList* cl = dd->CmdLists[n];
        memcpy(vtx_dst, cl->VtxBuffer.Data, cl->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cl->IdxBuffer.Data, cl->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cl->VtxBuffer.Size;
        idx_dst += cl->IdxBuffer.Size;

        for (int c = 0; c < cl->CmdBuffer.Size; ++c)
        {
            const ImDrawCmd& pcmd = cl->CmdBuffer[c];
            ui_draw_cmd dc;
            dc.clip[0] = pcmd.ClipRect.x;
            dc.clip[1] = pcmd.ClipRect.y;
            dc.clip[2] = pcmd.ClipRect.z;
            dc.clip[3] = pcmd.ClipRect.w;
            dc.elem_count = pcmd.ElemCount;
            dc.idx_offset = idx_run;
            dc.vtx_offset = vtx_base;
            s.cmds.push_back(dc);
            idx_run += pcmd.ElemCount;
        }
        vtx_base += cl->VtxBuffer.Size;
    }

    s.total_vtx = static_cast<uint32_t>(dd->TotalVtxCount);
    s.total_idx = static_cast<uint32_t>(dd->TotalIdxCount);
    s.valid = true;
}

void
vulkan_render::update_ui(frame_state& fs)
{
    KRG_check_render_thread();
    // Read this frame's snapshot slot, NOT the live ImGui draw data — the main
    // thread may already be building the next frame into ImGui's single buffer.
    const ui_draw_snapshot& s = m_ui_snapshots[m_draw_frame_slot];

    if (!s.valid || s.total_vtx == 0 || s.total_idx == 0)
    {
        return;
    }

    const VkDeviceSize vertex_buffer_size = s.vtx.size();
    const VkDeviceSize index_buffer_size = s.idx.size();

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

    fs.ui.vertex_count = static_cast<int32_t>(s.total_vtx);
    fs.ui.index_count = static_cast<int32_t>(s.total_idx);

    fs.ui.vertex_buffer.begin();
    fs.ui.index_buffer.begin();

    auto* vtx_dst = fs.ui.vertex_buffer.allocate_data((uint32_t)vertex_buffer_size);
    auto* idx_dst = fs.ui.index_buffer.allocate_data((uint32_t)index_buffer_size);
    memcpy(vtx_dst, s.vtx.data(), vertex_buffer_size);
    memcpy(idx_dst, s.idx.data(), index_buffer_size);

    fs.ui.vertex_buffer.end();
    fs.ui.index_buffer.end();

    fs.ui.vertex_buffer.flush();
    fs.ui.index_buffer.flush();
}
#endif  // KRG_HAS_IMGUI

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

    m_scene_upscale_txt = create_texture(
        AID("scene_lowres_txt"), m_scene_lowres_images[0], m_scene_lowres_views[0]);

    std::vector<texture_sampler_data> samples(1);
    samples.front().texture = m_scene_upscale_txt;
    samples.front().slot = 0;

    m_scene_upscale_mat =
        create_material(AID("mat_scene_upscale"),
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

#include "vulkan_render/kryga_render.h"

#include <tracy/Tracy.hpp>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_compute_shader_data.h"

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_frustum_types.h>
#include <gpu_types/gpu_shadow_types.h>

#include <utils/kryga_log.h>
#include <utils/check.h>

#include <global_state/global_state.h>

#include <cmath>

namespace kryga
{
namespace render
{
namespace
{

void*
ensure_buffer_capacity_and_map(vk_utils::vulkan_buffer& buffer,
                               size_t required_size,
                               const char* name)
{
    KRG_check(required_size, "Should never happen");

    if (required_size >= buffer.get_alloc_size())
    {
        auto old_buffer = std::move(buffer);

        buffer = glob::glob_state().getr_render_device().create_buffer(
            required_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        ALOG_INFO("Reallocating {0} buffer {1} => {2}",
                  name,
                  old_buffer.get_alloc_size(),
                  buffer.get_alloc_size());

        old_buffer.begin();
        buffer.begin();

        memcpy(buffer.get_data(), old_buffer.get_data(), old_buffer.get_alloc_size());

        old_buffer.end();
    }
    else
    {
        buffer.begin();
    }

    return buffer.allocate_data((uint32_t)required_size);
}

}  // namespace

// ============================================================================
// Data Upload Functions
// ============================================================================

void
vulkan_render::upload_obj_data(render::frame_state& frame)
{
    const auto total_size = m_cache.objects.get_size() * sizeof(gpu::object_data);

    auto* data = (gpu::object_data*)ensure_buffer_capacity_and_map(
        frame.buffers.objects, total_size, "objects");
    KRG_check(data, "Should never happen");

    upload_gpu_object_data(data);
    frame.buffers.objects.end();
}

void
vulkan_render::upload_universal_light_data(render::frame_state& frame)
{
    const auto total_size = m_cache.universal_lights.get_size() * sizeof(gpu::universal_light_data);

    auto* data = (gpu::universal_light_data*)ensure_buffer_capacity_and_map(
        frame.buffers.universal_lights, total_size, "universal lights");
    KRG_check(data, "Should never happen");

    upload_gpu_universal_light_data(data);
    frame.buffers.universal_lights.end();
}

void
vulkan_render::upload_directional_light_data(render::frame_state& frame)
{
    const auto total_size =
        m_cache.directional_lights.get_size() * sizeof(gpu::directional_light_data);

    auto* data = (gpu::directional_light_data*)ensure_buffer_capacity_and_map(
        frame.buffers.directional_lights, total_size, "directional lights");

    KRG_check(data, "Should never happen");

    upload_gpu_directional_light_data(data);
    frame.buffers.directional_lights.end();
}

void
vulkan_render::upload_material_data(render::frame_state& frame)
{
    auto total_size = m_materials_layout.calc_new_size();

    bool reallocated = false;

    vk_utils::vulkan_buffer old_buffer_tb;

    if (total_size >= frame.buffers.materials.get_alloc_size())
    {
        old_buffer_tb = std::move(frame.buffers.materials);

        frame.buffers.materials = glob::glob_state().getr_render_device().create_buffer(
            total_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        ALOG_INFO("Reallocating material buffer {0} => {1}",
                  old_buffer_tb.get_alloc_size(),
                  frame.buffers.materials.get_alloc_size());

        reallocated = true;
    }

    if (reallocated)
    {
        old_buffer_tb.begin();
    }

    frame.buffers.materials.begin();

    for (auto i = 0; i < m_materials_layout.get_segments_size(); ++i)
    {
        auto& s = m_materials_layout.at(i);

        auto src_offset = s.offset;
        auto size = s.get_allocated_size();

        m_materials_layout.update_segment(i);

        if (m_materials_layout.dirty_layout())
        {
            auto dst_offset = s.offset;

            if (reallocated)
            {
                memcpy(frame.buffers.materials.get_data() + dst_offset,
                       old_buffer_tb.get_data() + src_offset,
                       size);
            }
            else
            {
                memmove(frame.buffers.materials.get_data() + dst_offset,
                        old_buffer_tb.get_data() + src_offset,
                        size);
            }
        }
    }

    if (reallocated)
    {
        old_buffer_tb.end();
    }

    auto mat_begin = frame.buffers.materials.get_data();

    for (int i = 0; i < m_materials_layout.get_segments_size(); ++i)
    {
        auto& sm = m_materials_layout.at(i);
        auto& mat_set_queue = frame.uploads.materials_queue_set[sm.index];

        upload_gpu_materials_data(mat_begin + sm.offset, mat_set_queue);
    }

    frame.buffers.materials.end();
    m_materials_layout.reset_dirty_layout();
}

void
vulkan_render::upload_bone_matrices(render::frame_state& frame)
{
    // Always upload at least one identity matrix so the SSBO is valid
    if (m_bone_matrices_staging.empty())
    {
        m_bone_matrices_staging.push_back(glm::mat4(1.0f));
    }

    const size_t required_size = m_bone_matrices_staging.size() * sizeof(glm::mat4);

    // Regrow if needed
    if (required_size >= frame.buffers.bone_matrices.get_alloc_size())
    {
        auto old_buffer = std::move(frame.buffers.bone_matrices);
        frame.buffers.bone_matrices = glob::glob_state().getr_render_device().create_buffer(
            required_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        ALOG_INFO("Reallocating bone_matrices buffer {} => {}",
                  old_buffer.get_alloc_size(),
                  frame.buffers.bone_matrices.get_alloc_size());
    }

    frame.buffers.bone_matrices.begin();
    auto* dst = (glm::mat4*)frame.buffers.bone_matrices.allocate_data((uint32_t)required_size);
    memcpy(dst, m_bone_matrices_staging.data(), required_size);
    frame.buffers.bone_matrices.end();
}

void
vulkan_render::schd_update_material(render::material_data* md)
{
    for (auto& q : m_frames)
    {
        q.uploads.materials_queue_set[md->gpu_type_idx()].emplace_back(md);
        q.uploads.has_pending_materials = true;
    }
}

void
vulkan_render::schd_add_light(render::vulkan_directional_light_data* ld)
{
    for (auto& q : m_frames)
    {
        q.uploads.directional_light_queue.emplace_back(ld);
    }
}

void
vulkan_render::schd_add_light(render::vulkan_universal_light_data* ld)
{
    m_clusters_dirty = true;
    m_light_grid_dirty = true;
    for (auto& q : m_frames)
    {
        q.uploads.universal_light_queue.emplace_back(ld);
    }
}

void
vulkan_render::schd_update_light(render::vulkan_directional_light_data* ld)
{
    for (auto& q : m_frames)
    {
        q.uploads.directional_light_queue.emplace_back(ld);
    }
}

void
vulkan_render::schd_update_light(render::vulkan_universal_light_data* ld)
{
    m_clusters_dirty = true;
    m_light_grid_dirty = true;
    for (auto& q : m_frames)
    {
        q.uploads.universal_light_queue.emplace_back(ld);
    }
}

void
vulkan_render::set_selected_directional_light(const utils::id& id)
{
    m_selected_directional_light_id = id;
}

uint32_t
vulkan_render::get_selected_directional_light_slot()
{
    if (m_selected_directional_light_id.valid())
    {
        auto* rh = m_cache.directional_lights.find_by_id(m_selected_directional_light_id);
        if (rh)
        {
            return rh->slot();
        }
    }
    // Fallback: use first light if available
    if (m_cache.directional_lights.get_actual_size() > 0)
    {
        return m_cache.directional_lights.at(0)->slot();
    }
    return 0;
}

void
vulkan_render::clear_upload_queue()
{
    for (auto& q : m_frames)
    {
        q.uploads.clear_all();
    }
}

void
vulkan_render::upload_gpu_object_data(gpu::object_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().uploads.objects_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        object_SSBO[obj->slot()] = obj->gpu_data;
    }
}

void
vulkan_render::upload_gpu_universal_light_data(gpu::universal_light_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().uploads.universal_light_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        obj->gpu_data.slot = obj->slot();  // Ensure slot is synced for GPU access
        object_SSBO[obj->slot()] = obj->gpu_data;
    }
}

void
vulkan_render::upload_gpu_directional_light_data(gpu::directional_light_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().uploads.directional_light_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        object_SSBO[obj->slot()] = obj->gpu_data;
    }
}

void
vulkan_render::upload_gpu_materials_data(uint8_t* ssbo_data, materials_update_queue& mats_to_update)
{
    if (mats_to_update.empty())
    {
        return;
    }

    for (auto& m : mats_to_update)
    {
        auto dst_ptr = ssbo_data + m->gpu_idx() * m->get_gpu_data().size();
        memcpy(dst_ptr, m->get_gpu_data().data(), m->get_gpu_data().size());
    }

    mats_to_update.clear();
}

// ============================================================================
// Clustered Lighting
// ============================================================================

void
vulkan_render::build_light_clusters()
{
    KRG_check(m_cluster_grid.is_initialized(), "Should always be here");

    // Early-out: no universal lights means no cluster work needed
    if (m_cache.universal_lights.get_size() == 0)
    {
        m_cluster_grid.clear();
        return;
    }

    // Build light info list from cache (skip invalid/freed lights)
    std::vector<cluster_light_info> lights;
    lights.reserve(m_cache.universal_lights.get_actual_size());

    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
        {
            continue;
        }
        // Add small margin for cluster assignment to avoid edge cases
        lights.push_back({light->slot(), light->gpu_data.position, light->gpu_data.radius * 1.05f});
    }

    // Compute view and projection matrices for cluster building
    glm::mat4 inv_projection = glm::inverse(m_camera_data.projection);

    // Build clusters
    m_cluster_grid.build_clusters(
        m_camera_data.view, m_camera_data.projection, inv_projection, lights);
}

void
vulkan_render::rebuild_light_grid()
{
    KRG_check(m_light_grid.is_initialized(), "Light grid should be initialized");

    m_light_grid.clear();

    // Insert all universal lights into the grid (skip invalid/freed lights)
    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
        {
            continue;
        }
        m_light_grid.insert_light(light->slot(), light->gpu_data.position, light->gpu_data.radius);
    }
}

void
vulkan_render::upload_cluster_data(render::frame_state& frame)
{
    ZoneScopedN("Render::UploadClusters");

    if (!m_cluster_grid.is_initialized())
    {
        return;
    }

    const auto& config = m_cluster_grid.get_config();

    // Upload cluster light counts (scalar layout — direct uint32_t array)
    {
        const auto& counts = m_cluster_grid.get_cluster_light_counts();
        const size_t size = counts.size() * sizeof(uint32_t);

        frame.buffers.cluster_counts.begin();
        auto* data = frame.buffers.cluster_counts.allocate_data((uint32_t)size);
        memcpy(data, counts.data(), size);
        frame.buffers.cluster_counts.end();
    }

    // Upload cluster light indices (scalar layout — direct uint32_t array)
    {
        const auto& indices = m_cluster_grid.get_cluster_light_indices();
        const size_t size = indices.size() * sizeof(uint32_t);

        frame.buffers.cluster_indices.begin();
        auto* data = frame.buffers.cluster_indices.allocate_data((uint32_t)size);
        memcpy(data, indices.data(), size);
        frame.buffers.cluster_indices.end();
    }

    // Upload cluster config
    {
        // Update config from current camera/grid state
        m_cluster_config.tiles_x = config.tiles_x;
        m_cluster_config.tiles_y = config.tiles_y;
        m_cluster_config.depth_slices = config.depth_slices;
        m_cluster_config.tile_size = config.tile_size;
        m_cluster_config.near_plane = config.near_plane;
        m_cluster_config.far_plane = config.far_plane;
        m_cluster_config.log_depth_ratio = std::log(config.far_plane / config.near_plane);
        m_cluster_config.max_lights_per_cluster = config.max_lights_per_cluster;
        m_cluster_config.screen_width = config.screen_width;
        m_cluster_config.screen_height = config.screen_height;

        frame.buffers.cluster_config.begin();
        auto* data = frame.buffers.cluster_config.allocate_data(sizeof(gpu::cluster_grid_data));
        memcpy(data, &m_cluster_config, sizeof(gpu::cluster_grid_data));
        frame.buffers.cluster_config.end();
    }
}

// ============================================================================
// Compute Pass Dispatch
// ============================================================================

void
vulkan_render::dispatch_cluster_cull_impl(VkCommandBuffer cmd)
{
    ZoneScopedN("Render::DispatchClusterCullImpl");

    // Bind compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cluster_cull_shader->m_pipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_cluster_cull_shader->m_pipeline_layout,
                            0,
                            1,
                            &m_cluster_cull_descriptor_set,
                            0,
                            nullptr);

    // Push light count
    uint32_t num_lights = m_cache.universal_lights.get_size();
    vkCmdPushConstants(cmd,
                       m_cluster_cull_shader->m_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(uint32_t),
                       &num_lights);

    // Calculate dispatch size
    const auto& config = m_cluster_grid.get_config();
    uint32_t total_clusters = config.tiles_x * config.tiles_y * config.depth_slices;
    uint32_t workgroup_size = 64;  // Must match local_size_x in shader
    uint32_t num_workgroups = (total_clusters + workgroup_size - 1) / workgroup_size;

    // Dispatch compute shader
    vkCmdDispatch(cmd, num_workgroups, 1, 1);

    // Note: Barrier is handled by render graph, not here
}

// ============================================================================
// GPU Frustum Culling
// ============================================================================

void
vulkan_render::upload_frustum_data(render::frame_state& frame)
{
    ZoneScopedN("Render::UploadFrustumData");

    // Convert frustum planes to GPU format (vec4: normal.xyz, distance)
    gpu::frustum_data frustum_gpu;
    for (int i = 0; i < 6; ++i)
    {
        const auto& plane = m_frustum.get_plane(static_cast<frustum::plane_id>(i));
        frustum_gpu.planes[i] = glm::vec4(plane.normal, plane.distance);
    }

    // Upload to buffer
    frame.buffers.frustum_data.begin();
    auto* data = frame.buffers.frustum_data.allocate_data(sizeof(gpu::frustum_data));
    memcpy(data, &frustum_gpu, sizeof(gpu::frustum_data));
    frame.buffers.frustum_data.end();
}

void
vulkan_render::dispatch_frustum_cull_impl(VkCommandBuffer cmd)
{
    ZoneScopedN("Render::DispatchFrustumCullImpl");

    // Frustum culling is required for instanced mode - assert instead of early return
    KRG_check(m_frustum_cull_shader, "Frustum cull shader required for instanced mode");
    KRG_check(m_gpu_frustum_culling_enabled, "GPU frustum culling required for instanced mode");

    auto& current_frame = *m_current_frame;

    // Clear the visible count to 0 before culling
    vkCmdFillBuffer(cmd, current_frame.buffers.cull_output.buffer(), 0, sizeof(uint32_t), 0);

    // Memory barrier to ensure fill is complete before compute
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         1,
                         &barrier,
                         0,
                         nullptr,
                         0,
                         nullptr);

    // Bind compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_frustum_cull_shader->m_pipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_frustum_cull_shader->m_pipeline_layout,
                            0,
                            1,
                            &m_frustum_cull_descriptor_set,
                            0,
                            nullptr);

    // Push constants: object count and max visible
    struct FrustumCullPushConstants
    {
        uint32_t object_count;
        uint32_t max_visible;
    } pc;

    pc.object_count = static_cast<uint32_t>(m_cache.objects.get_size());
    pc.max_visible = static_cast<uint32_t>(m_cache.objects.get_size());  // Same capacity

    vkCmdPushConstants(cmd,
                       m_frustum_cull_shader->m_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(pc),
                       &pc);

    // Calculate dispatch size
    uint32_t workgroup_size = 64;  // Must match local_size_x in shader
    uint32_t num_workgroups = (pc.object_count + workgroup_size - 1) / workgroup_size;

    if (num_workgroups > 0)
    {
        vkCmdDispatch(cmd, num_workgroups, 1, 1);
    }

    // Note: Barrier to graphics is handled by render graph
}

}  // namespace render
}  // namespace kryga

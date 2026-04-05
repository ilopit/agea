#include "vulkan_render/kryga_render.h"
#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"

#include <tracy/Tracy.hpp>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_compute_shader_data.h"

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_frustum_types.h>
#include <gpu_types/gpu_shadow_types.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <vfs/io.h>
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

    // std140 padded struct for cluster data (16 bytes per element)
    struct alignas(16) cluster_data_std140
    {
        uint32_t value;
    };

    // Upload cluster light counts (padded to std140)
    {
        const auto& counts = m_cluster_grid.get_cluster_light_counts();
        const size_t size = counts.size() * sizeof(cluster_data_std140);

        frame.buffers.cluster_counts.begin();
        auto* data = reinterpret_cast<cluster_data_std140*>(
            frame.buffers.cluster_counts.allocate_data((uint32_t)size));

        for (size_t i = 0; i < counts.size(); ++i)
        {
            data[i].value = counts[i];
        }
        frame.buffers.cluster_counts.end();
    }

    // Upload cluster light indices (padded to std140)
    {
        const auto& indices = m_cluster_grid.get_cluster_light_indices();
        const size_t size = indices.size() * sizeof(cluster_data_std140);

        frame.buffers.cluster_indices.begin();
        auto* data = reinterpret_cast<cluster_data_std140*>(
            frame.buffers.cluster_indices.allocate_data((uint32_t)size));

        for (size_t i = 0; i < indices.size(); ++i)
        {
            data[i].value = indices[i];
        }
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

void
vulkan_render::init_cluster_cull_compute()
{
    ZoneScopedN("Render::InitClusterCullCompute");

    kryga::utils::buffer shader_buffer;
    if (!vfs::load_buffer(vfs::rid("data://shaders_includes/cluster_cull.comp"), shader_buffer))
    {
        ALOG_WARN("Failed to load cluster_cull.comp - GPU cluster culling disabled");
        return;
    }

    // Create compute pass - bindings are owned by the pass, shader is created through it
    m_cluster_cull_pass = std::make_shared<render_pass>(AID("cluster_cull"), rg_pass_type::compute);

    // Declare bindings for cluster cull compute shader on the pass
    // Names must match render graph resource names (dyn_ prefix)
    // set=0, binding=0: ClusterConfig (uniform)
    // set=0, binding=1: CameraData (uniform)
    // set=0, binding=2: LightBuffer (storage, readonly)
    // set=0, binding=3: ClusterLightCounts (storage, writeonly)
    // set=0, binding=4: ClusterLightIndices (storage, writeonly)
    m_cluster_cull_pass->bindings()
        .add(AID("dyn_cluster_config"),
             0,
             0,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_camera_data"),
             0,
             1,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_gpu_universal_light_data"),
             0,
             2,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_cluster_light_counts"),
             0,
             3,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_cluster_light_indices"),
             0,
             4,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT);

    m_cluster_cull_pass->finalize_bindings(
        *glob::glob_state().getr_render_device().descriptor_layout_cache());

    // Create compute shader through the pass
    compute_shader_create_info info;
    info.shader_buffer = &shader_buffer;

    auto rc = m_cluster_cull_pass->create_compute_shader(
        AID("cluster_cull"), info, m_cluster_cull_shader);
    if (rc != result_code::ok)
    {
        ALOG_WARN("Failed to create cluster cull compute shader - GPU cluster culling disabled");
        m_cluster_cull_pass.reset();
        m_cluster_cull_shader = nullptr;
        return;
    }

    ALOG_INFO("GPU cluster culling compute shader initialized");
}

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
vulkan_render::init_frustum_cull_compute()
{
    ZoneScopedN("Render::InitFrustumCullCompute");

    kryga::utils::buffer shader_buffer;
    if (!vfs::load_buffer(vfs::rid("data://shaders_includes/frustum_cull.comp"), shader_buffer))
    {
        ALOG_WARN("Failed to load frustum_cull.comp - GPU frustum culling disabled");
        m_gpu_frustum_culling_enabled = false;
        return;
    }

    // Create compute pass - bindings are owned by the pass
    m_frustum_cull_pass = std::make_shared<render_pass>(AID("frustum_cull"), rg_pass_type::compute);

    // Declare bindings for frustum cull compute shader
    // set=0, binding=0: FrustumBuffer (uniform)
    // set=0, binding=1: ObjectBuffer (storage, readonly)
    // set=0, binding=2: VisibleIndices (storage, writeonly)
    // set=0, binding=3: CullOutput (storage)
    m_frustum_cull_pass->bindings()
        .add(AID("dyn_frustum_data"),
             0,
             0,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_object_buffer"),
             0,
             1,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_visible_indices"),
             0,
             2,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_cull_output"),
             0,
             3,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT);

    m_frustum_cull_pass->finalize_bindings(
        *glob::glob_state().getr_render_device().descriptor_layout_cache());

    // Create compute shader through the pass
    compute_shader_create_info info;
    info.shader_buffer = &shader_buffer;

    auto rc = m_frustum_cull_pass->create_compute_shader(
        AID("frustum_cull"), info, m_frustum_cull_shader);
    if (rc != result_code::ok)
    {
        ALOG_WARN("Failed to create frustum cull compute shader - GPU frustum culling disabled");
        m_frustum_cull_pass.reset();
        m_frustum_cull_shader = nullptr;
        m_gpu_frustum_culling_enabled = false;
        return;
    }

    m_gpu_frustum_culling_enabled = true;
    ALOG_INFO("GPU frustum culling compute shader initialized");
}

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

// ============================================================================
// Render Graph Setup
// ============================================================================

void
vulkan_render::setup_render_graph()
{
    switch (m_render_mode)
    {
    case render_mode::instanced:
        setup_instanced_render_graph();
        break;
    case render_mode::per_object:
        setup_per_object_render_graph();
        break;
    }
}

void
vulkan_render::setup_instanced_render_graph()
{
    // =========================================================================
    // INSTANCED MODE GRAPH
    // - GPU cluster culling compute pass
    // - Batched instanced drawing
    // - instance_slots buffer maps gl_InstanceIndex -> object slot
    // =========================================================================

    // Register resources
    m_render_graph.register_buffer(AID("dyn_camera_data"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_object_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_gpu_universal_light_data"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_directional_lights_buffer"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_counts"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_indices"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_instance_slots"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_bone_matrices"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_material_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_shadow_data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // GPU frustum culling buffers
    m_render_graph.register_buffer(AID("dyn_frustum_data"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_visible_indices"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cull_output"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    m_render_graph.import_resource(AID("swapchain"), rg_resource_type::image);
    m_render_graph.import_resource(AID("ui_target"), rg_resource_type::image);
    m_render_graph.import_resource(AID("picking_target"), rg_resource_type::image);

    // Shadow passes (CSM cascades)
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        auto pass_name = AID("shadow_csm_" + std::to_string(c));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_passes[c].get(),
                                         VkClearColorValue{},
                                         [this, c](VkCommandBuffer cmd)
                                         { draw_shadow_pass(cmd, c); });
    }

    // Local light shadow passes: front hemisphere (spot + point front)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        auto pass_name = AID("shadow_local_" + std::to_string(i));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, false); });
    }

    // Local light shadow passes: back hemisphere (point lights DPSM only)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        auto pass_name = AID("shadow_local_back_" + std::to_string(i));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2 + 1].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, true); });
    }

    // Compute pass: GPU frustum culling (runs before cluster culling)
    // Frustum culling is required for instanced mode - dispatch_frustum_cull_impl asserts if not
    // ready
    m_render_graph.add_compute_pass(AID("frustum_cull"),
                                    {m_render_graph.read(AID("dyn_frustum_data")),
                                     m_render_graph.read(AID("dyn_object_buffer")),
                                     m_render_graph.write(AID("dyn_visible_indices")),
                                     m_render_graph.write(AID("dyn_cull_output"))},
                                    [this](VkCommandBuffer cmd)
                                    { dispatch_frustum_cull_impl(cmd); });

    // Compute pass: GPU cluster culling
    m_render_graph.add_compute_pass(AID("cluster_cull"),
                                    {m_render_graph.write(AID("dyn_cluster_light_counts")),
                                     m_render_graph.write(AID("dyn_cluster_light_indices")),
                                     m_render_graph.read(AID("dyn_gpu_universal_light_data"))},
                                    [this](VkCommandBuffer cmd)
                                    {
                                        if (m_cluster_cull_shader)
                                        {
                                            dispatch_cluster_cull_impl(cmd);
                                        }
                                    });

    // UI pass
    m_render_graph.add_graphics_pass(AID("ui"),
                                     {m_render_graph.write(AID("ui_target"))},
                                     get_render_pass(AID("ui")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer) { draw_ui(*m_current_frame); });

    // Picking pass - instanced batched drawing
    m_render_graph.add_graphics_pass(AID("picking"),
                                     {m_render_graph.write(AID("picking_target")),
                                      m_render_graph.read(AID("dyn_camera_data")),
                                      m_render_graph.read(AID("dyn_object_buffer")),
                                      m_render_graph.read(AID("dyn_cluster_light_counts")),
                                      m_render_graph.read(AID("dyn_cluster_light_indices")),
                                      m_render_graph.read(AID("dyn_cluster_config")),
                                      m_render_graph.read(AID("dyn_instance_slots")),
                                      m_render_graph.read(AID("dyn_bone_matrices"))},
                                     get_render_pass(AID("picking")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer cmd) { draw_picking_instanced(cmd); });

    // Main pass - instanced batched drawing
    // Shadow maps are sampled via bindless textures. Declaring them as reads
    // ensures the render graph orders shadow passes before the main pass.
    {
        std::vector<rg_resource_ref> main_resources = {
            m_render_graph.write(AID("swapchain")),
            m_render_graph.read(AID("ui_target")),
            m_render_graph.read(AID("dyn_camera_data")),
            m_render_graph.read(AID("dyn_object_buffer")),
            m_render_graph.read(AID("dyn_gpu_universal_light_data")),
            m_render_graph.read(AID("dyn_directional_lights_buffer")),
            m_render_graph.read(AID("dyn_cluster_light_counts")),
            m_render_graph.read(AID("dyn_cluster_light_indices")),
            m_render_graph.read(AID("dyn_cluster_config")),
            m_render_graph.read(AID("dyn_instance_slots")),
            m_render_graph.read(AID("dyn_bone_matrices")),
            m_render_graph.read(AID("dyn_material_buffer")),
            m_render_graph.read(AID("dyn_shadow_data")),
        };

        // Shadow map dependencies (ordering only — actual sampling is via bindless)
        for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
        {
            main_resources.push_back(
                m_render_graph.read(AID("shadow_csm_" + std::to_string(c))));
        }
        for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
        {
            main_resources.push_back(
                m_render_graph.read(AID("shadow_local_" + std::to_string(i))));
            main_resources.push_back(
                m_render_graph.read(AID("shadow_local_back_" + std::to_string(i))));
        }

        m_render_graph.add_graphics_pass(AID("main"),
                                         std::move(main_resources),
                                         get_render_pass(AID("main")),
                                         VkClearColorValue{0, 0, 0, 1.0},
                                         [this](VkCommandBuffer)
                                         { draw_objects_instanced(*m_current_frame); });
    }

    bool result = m_render_graph.compile();
    KRG_check(result, "Instanced render graph compilation failed");
}

void
vulkan_render::setup_per_object_render_graph()
{
    // =========================================================================
    // PER-OBJECT MODE GRAPH
    // - No compute pass (CPU light grid used instead)
    // - Per-object draw calls with firstInstance = slot
    // - Identity buffer: slots[i] = i
    // =========================================================================

    // Register resources (same as instanced, but cluster buffers are CPU-filled)
    m_render_graph.register_buffer(AID("dyn_camera_data"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_object_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_gpu_universal_light_data"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_directional_lights_buffer"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_counts"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_indices"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_instance_slots"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_bone_matrices"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_material_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_shadow_data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    m_render_graph.import_resource(AID("swapchain"), rg_resource_type::image);
    m_render_graph.import_resource(AID("ui_target"), rg_resource_type::image);
    m_render_graph.import_resource(AID("picking_target"), rg_resource_type::image);

    // Shadow passes (CSM cascades)
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        auto pass_name = AID("shadow_csm_" + std::to_string(c));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_passes[c].get(),
                                         VkClearColorValue{},
                                         [this, c](VkCommandBuffer cmd)
                                         { draw_shadow_pass(cmd, c); });
    }

    // Local light shadow passes: front + back hemispheres
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        auto front_name = AID("shadow_local_" + std::to_string(i));
        m_render_graph.import_resource(front_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(front_name,
                                         {m_render_graph.write(front_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, false); });

        auto back_name = AID("shadow_local_back_" + std::to_string(i));
        m_render_graph.import_resource(back_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(back_name,
                                         {m_render_graph.write(back_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2 + 1].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, true); });
    }

    // NO compute pass - per-object mode uses CPU light grid

    // UI pass
    m_render_graph.add_graphics_pass(AID("ui"),
                                     {m_render_graph.write(AID("ui_target"))},
                                     get_render_pass(AID("ui")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer) { draw_ui(*m_current_frame); });

    // Picking pass - per-object drawing
    m_render_graph.add_graphics_pass(AID("picking"),
                                     {m_render_graph.write(AID("picking_target")),
                                      m_render_graph.read(AID("dyn_camera_data")),
                                      m_render_graph.read(AID("dyn_object_buffer")),
                                      m_render_graph.read(AID("dyn_cluster_light_counts")),
                                      m_render_graph.read(AID("dyn_cluster_light_indices")),
                                      m_render_graph.read(AID("dyn_cluster_config")),
                                      m_render_graph.read(AID("dyn_instance_slots")),
                                      m_render_graph.read(AID("dyn_bone_matrices"))},
                                     get_render_pass(AID("picking")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer cmd) { draw_picking_per_object(cmd); });

    // Main pass - per-object drawing (see instanced graph for shadow map note)
    m_render_graph.add_graphics_pass(AID("main"),
                                     {m_render_graph.write(AID("swapchain")),
                                      m_render_graph.read(AID("ui_target")),
                                      m_render_graph.read(AID("dyn_camera_data")),
                                      m_render_graph.read(AID("dyn_object_buffer")),
                                      m_render_graph.read(AID("dyn_gpu_universal_light_data")),
                                      m_render_graph.read(AID("dyn_directional_lights_buffer")),
                                      m_render_graph.read(AID("dyn_cluster_light_counts")),
                                      m_render_graph.read(AID("dyn_cluster_light_indices")),
                                      m_render_graph.read(AID("dyn_cluster_config")),
                                      m_render_graph.read(AID("dyn_instance_slots")),
                                      m_render_graph.read(AID("dyn_bone_matrices")),
                                      m_render_graph.read(AID("dyn_material_buffer")),
                                      m_render_graph.read(AID("dyn_shadow_data"))},
                                     get_render_pass(AID("main")),
                                     VkClearColorValue{0, 0, 0, 1.0},
                                     [this](VkCommandBuffer)
                                     { draw_objects_per_object(*m_current_frame); });

    bool result = m_render_graph.compile();
    KRG_check(result, "Per-object render graph compilation failed");
}

}  // namespace render
}  // namespace kryga

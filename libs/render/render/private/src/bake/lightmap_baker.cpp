#include "vulkan_render/bake/lightmap_baker.h"

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/vk_descriptors.h>
#include <vulkan_render/utils/vulkan_initializers.h>
#include <vulkan_render/utils/vulkan_image.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/types/vulkan_compute_shader_data.h>
#include <vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h>
#include <vulkan_render/vulkan_render_loader_create_infos.h>

#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/io.h>
#include <utils/kryga_log.h>

#include <render/utils/image_compare.h>

#include <vk_mem_alloc.h>

#include <chrono>
#include <cmath>

namespace kryga
{
namespace render
{

void
lightmap_baker::add_mesh(const gpu::vertex_data* vertices,
                         uint32_t vertex_count,
                         const uint32_t* indices,
                         uint32_t index_count)
{
    uint32_t base_vertex = static_cast<uint32_t>(m_vertices.size());

    m_vertices.insert(m_vertices.end(), vertices, vertices + vertex_count);

    for (uint32_t i = 0; i < index_count; ++i)
    {
        m_indices.push_back(indices[i] + base_vertex);
    }
}

void
lightmap_baker::clear()
{
    m_vertices.clear();
    m_indices.clear();
    m_lightmap_data.clear();
    m_ao_data.clear();
}

namespace
{

vk_utils::vulkan_buffer
create_storage_buffer(render_device& device, const void* data, size_t size)
{
    auto staging = device.create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VMA_MEMORY_USAGE_CPU_ONLY);

    void* mapped = nullptr;
    vmaMapMemory(device.allocator(), staging.allocation(), &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(device.allocator(), staging.allocation());

    auto gpu_buf = device.create_buffer(
        size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy copy{};
            copy.size = size;
            vkCmdCopyBuffer(cmd, staging.buffer(), gpu_buf.buffer(), 1, &copy);
        });

    return gpu_buf;
}

vk_utils::vulkan_buffer
create_uniform_buffer(render_device& device, const void* data, size_t size)
{
    auto buf = device.create_buffer(
        size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);

    void* mapped = nullptr;
    vmaMapMemory(device.allocator(), buf.allocation(), &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(device.allocator(), buf.allocation());

    return buf;
}

vk_utils::vulkan_image_sptr
create_storage_image(render_device& device, uint32_t w, uint32_t h, VkFormat format)
{
    VkExtent3D extent{w, h, 1};
    auto ci = vk_utils::make_image_create_info(
        format,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        extent);

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto img = vk_utils::vulkan_image::create(device.get_vma_allocator_provider(), ci, aci);

    // Transition to GENERAL layout for compute shader access
    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vk_utils::make_insert_image_memory_barrier(
                cmd, img.image(), 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, range);
        });

    return std::make_shared<vk_utils::vulkan_image>(std::move(img));
}

void
insert_compute_barrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);
}

}  // namespace

bake::bake_result
lightmap_baker::bake(const bake::bake_settings& settings)
{
    bake::bake_result result;

    auto start = std::chrono::high_resolution_clock::now();

    if (m_vertices.empty() || m_indices.empty())
    {
        ALOG_ERROR("lightmap_baker: no meshes added");
        return result;
    }

    auto* device = glob::glob_state().get_render_device();
    if (!device)
    {
        ALOG_ERROR("lightmap_baker: no render device");
        return result;
    }

    // =====================================================================
    // Step 1: Build BVH
    // =====================================================================
    ALOG_INFO("lightmap_baker: building BVH for {} verts, {} indices",
              m_vertices.size(), m_indices.size());

    auto bvh = bake::build_bvh(m_vertices.data(),
                                static_cast<uint32_t>(m_vertices.size()),
                                m_indices.data(),
                                static_cast<uint32_t>(m_indices.size()));

    if (bvh.nodes.empty())
    {
        ALOG_ERROR("lightmap_baker: BVH build failed");
        return result;
    }

    result.total_triangles = static_cast<uint32_t>(bvh.triangles.size());
    result.total_nodes = static_cast<uint32_t>(bvh.nodes.size());

    ALOG_INFO("lightmap_baker: BVH — {} nodes, {} tris", bvh.nodes.size(), bvh.triangles.size());

    uint32_t W = settings.resolution;
    uint32_t H = settings.resolution;

    // =====================================================================
    // Step 2: Upload GPU resources
    // =====================================================================
    auto buf_bvh_nodes = create_storage_buffer(
        *device, bvh.nodes.data(), bvh.nodes.size() * sizeof(gpu::bvh_node));

    auto buf_triangles = create_storage_buffer(
        *device, bvh.triangles.data(), bvh.triangles.size() * sizeof(gpu::bake_triangle));

    gpu::bake_config config{};
    config.triangle_count = result.total_triangles;
    config.node_count = result.total_nodes;
    config.atlas_width = W;
    config.atlas_height = H;
    config.sample_count = settings.samples_per_texel;
    config.bounce_count = settings.bounce_count;
    config.ao_radius = settings.ao_radius;
    config.ao_intensity = settings.ao_intensity;

    auto buf_config = create_uniform_buffer(*device, &config, sizeof(gpu::bake_config));

    // =====================================================================
    // Step 3: Create GPU images
    // =====================================================================
    auto img_gbuf_pos = create_storage_image(*device, W, H, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto img_gbuf_normal = create_storage_image(*device, W, H, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto img_lightmap = create_storage_image(*device, W, H, VK_FORMAT_R16G16B16A16_SFLOAT);
    auto img_lightmap_bounce = create_storage_image(*device, W, H, VK_FORMAT_R16G16B16A16_SFLOAT);
    auto img_ao = create_storage_image(*device, W, H, VK_FORMAT_R16_SFLOAT);
    auto img_denoise_tmp = create_storage_image(*device, W, H, VK_FORMAT_R16G16B16A16_SFLOAT);

    // Create image views for descriptor binding
    auto make_view = [](vk_utils::vulkan_image_sptr& img, VkFormat fmt)
    {
        auto ci = vk_utils::make_imageview_create_info(fmt, img->image(), VK_IMAGE_ASPECT_COLOR_BIT);
        return vk_utils::vulkan_image_view::create_shared(ci);
    };

    auto view_gbuf_pos = make_view(img_gbuf_pos, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto view_gbuf_normal = make_view(img_gbuf_normal, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto view_lightmap = make_view(img_lightmap, VK_FORMAT_R16G16B16A16_SFLOAT);
    auto view_lightmap_bounce = make_view(img_lightmap_bounce, VK_FORMAT_R16G16B16A16_SFLOAT);
    auto view_ao = make_view(img_ao, VK_FORMAT_R16_SFLOAT);
    auto view_denoise_tmp = make_view(img_denoise_tmp, VK_FORMAT_R16G16B16A16_SFLOAT);

    // =====================================================================
    // Step 4: Create descriptor sets for each bake stage
    // =====================================================================
    auto& layout_cache = *device->descriptor_layout_cache();
    vk_utils::descriptor_allocator desc_alloc;

    // --- G-buffer rasterize descriptors (set 0) ---
    VkDescriptorBufferInfo tri_buf_info{buf_triangles.buffer(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo config_buf_info{buf_config.buffer(), 0, VK_WHOLE_SIZE};
    VkDescriptorImageInfo pos_img_info{VK_NULL_HANDLE, view_gbuf_pos->vk(), VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo norm_img_info{VK_NULL_HANDLE, view_gbuf_normal->vk(), VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorSet ds_gbuf;
    VkDescriptorSetLayout dsl_gbuf;
    vk_utils::descriptor_builder::begin(&layout_cache, &desc_alloc)
        .bind_buffer(0, &tri_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(1, &config_buf_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(2, 1, &pos_img_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(3, 1, &norm_img_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(ds_gbuf, dsl_gbuf);

    // --- Direct light descriptors ---
    VkDescriptorBufferInfo bvh_buf_info{buf_bvh_nodes.buffer(), 0, VK_WHOLE_SIZE};
    VkDescriptorImageInfo lightmap_img_info{VK_NULL_HANDLE, view_lightmap->vk(), VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo pos_read_info{VK_NULL_HANDLE, view_gbuf_pos->vk(), VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo norm_read_info{VK_NULL_HANDLE, view_gbuf_normal->vk(), VK_IMAGE_LAYOUT_GENERAL};

    // Scene must provide at least one directional light for baking
    KRG_check(!settings.directional_lights.empty(),
              "lightmap_baker: no directional lights provided in bake_settings");

    auto buf_dir_light = create_storage_buffer(
        *device, settings.directional_lights.data(),
        settings.directional_lights.size() * sizeof(gpu::directional_light_data));
    VkDescriptorBufferInfo light_buf_info{buf_dir_light.buffer(), 0, VK_WHOLE_SIZE};

    VkDescriptorSet ds_direct;
    VkDescriptorSetLayout dsl_direct;
    vk_utils::descriptor_builder::begin(&layout_cache, &desc_alloc)
        .bind_buffer(0, &bvh_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(1, &tri_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(2, &config_buf_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(3, &light_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(4, 1, &lightmap_img_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(5, 1, &pos_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(6, 1, &norm_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(ds_direct, dsl_direct);

    // --- Indirect bounce descriptors ---
    VkDescriptorImageInfo lightmap_read_info{VK_NULL_HANDLE, view_lightmap->vk(), VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo bounce_write_info{VK_NULL_HANDLE, view_lightmap_bounce->vk(), VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorSet ds_indirect;
    VkDescriptorSetLayout dsl_indirect;
    vk_utils::descriptor_builder::begin(&layout_cache, &desc_alloc)
        .bind_buffer(0, &bvh_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(1, &tri_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(2, &config_buf_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(3, 1, &lightmap_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(4, 1, &bounce_write_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(5, 1, &pos_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(6, 1, &norm_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(ds_indirect, dsl_indirect);

    // --- AO descriptors ---
    VkDescriptorImageInfo ao_img_info{VK_NULL_HANDLE, view_ao->vk(), VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorSet ds_ao;
    VkDescriptorSetLayout dsl_ao;
    vk_utils::descriptor_builder::begin(&layout_cache, &desc_alloc)
        .bind_buffer(0, &bvh_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(1, &tri_buf_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(2, &config_buf_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(3, 1, &ao_img_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(4, 1, &pos_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(5, 1, &norm_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(ds_ao, dsl_ao);

    // --- Denoise descriptors ---
    VkDescriptorImageInfo denoise_in_info{VK_NULL_HANDLE, view_lightmap->vk(), VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo denoise_out_info{VK_NULL_HANDLE, view_denoise_tmp->vk(), VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorSet ds_denoise;
    VkDescriptorSetLayout dsl_denoise;
    vk_utils::descriptor_builder::begin(&layout_cache, &desc_alloc)
        .bind_buffer(0, &config_buf_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(1, 1, &denoise_in_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(2, 1, &denoise_out_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(3, 1, &pos_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_image(4, 1, &norm_read_info, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(ds_denoise, dsl_denoise);

    // =====================================================================
    // Step 5: Create compute pipelines (runtime GLSL → SPIR-V via glslc)
    // =====================================================================

    // Helper: load shader source and compile via the engine's shader compiler
    struct bake_shader
    {
        compute_shader_data data{utils::id()};
        bool valid = false;
    };

    auto compile_bake_shader = [&](const char* vfs_path, const char* name) -> bake_shader
    {
        bake_shader s;
        s.data = compute_shader_data(AID(name));

        utils::buffer shader_buf;
        if (!vfs::load_buffer(vfs::rid(vfs_path), shader_buf))
        {
            ALOG_ERROR("lightmap_baker: failed to load {}", vfs_path);
            return s;
        }

        compute_shader_create_info info;
        info.shader_buffer = &shader_buf;

        auto rc = vulkan_compute_shader_loader::create_compute_shader(s.data, info);
        if (rc != result_code::ok)
        {
            ALOG_ERROR("lightmap_baker: failed to compile {}", vfs_path);
            return s;
        }

        s.valid = true;
        return s;
    };

    auto cs_gbuf = compile_bake_shader(
        "data://shaders_includes/bake/gbuffer_rasterize.comp", "bake_gbuf");
    auto cs_direct = compile_bake_shader(
        "data://shaders_includes/bake/lightmap_baker_direct.comp", "bake_direct");
    auto cs_indirect = compile_bake_shader(
        "data://shaders_includes/bake/lightmap_baker_indirect.comp", "bake_indirect");
    auto cs_ao = compile_bake_shader(
        "data://shaders_includes/bake/ao_baker.comp", "bake_ao");
    auto cs_denoise = compile_bake_shader(
        "data://shaders_includes/bake/lightmap_denoise.comp", "bake_denoise");

    if (!cs_gbuf.valid || !cs_direct.valid)
    {
        ALOG_ERROR("lightmap_baker: required compute shaders failed to compile");
        desc_alloc.cleanup();
        return result;
    }

    // =====================================================================
    // Step 6: Dispatch all bake stages
    // =====================================================================
    uint32_t wg_x = (W + 7) / 8;
    uint32_t wg_y = (H + 7) / 8;
    uint32_t tri_wg = (result.total_triangles + 63) / 64;

    device->immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            // --- G-buffer rasterization ---
            ALOG_INFO("lightmap_baker: dispatching G-buffer rasterization");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cs_gbuf.data.m_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    cs_gbuf.data.m_pipeline_layout, 0, 1, &ds_gbuf, 0, nullptr);
            vkCmdDispatch(cmd, tri_wg, 1, 1);
            insert_compute_barrier(cmd);

            // --- Direct lighting ---
            if (settings.bake_direct)
            {
                ALOG_INFO("lightmap_baker: dispatching direct lighting");
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cs_direct.data.m_pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        cs_direct.data.m_pipeline_layout, 0, 1, &ds_direct, 0,
                                        nullptr);
                vkCmdDispatch(cmd, wg_x, wg_y, 1);
                insert_compute_barrier(cmd);
            }

            // --- Indirect bounces ---
            if (settings.bake_indirect && !cs_indirect.valid)
            {
                ALOG_WARN("lightmap_baker: indirect bake requested but shader failed to compile, skipping");
            }
            if (settings.bake_indirect && cs_indirect.valid)
            {
                for (uint32_t bounce = 0; bounce < settings.bounce_count; ++bounce)
                {
                    ALOG_INFO("lightmap_baker: dispatching indirect bounce {}/{}",
                              bounce + 1, settings.bounce_count);

                    uint32_t seed = bounce * 7919u + 12347u;
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                      cs_indirect.data.m_pipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                            cs_indirect.data.m_pipeline_layout, 0, 1, &ds_indirect,
                                            0, nullptr);
                    vkCmdPushConstants(cmd, cs_indirect.data.m_pipeline_layout,
                                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &seed);
                    vkCmdDispatch(cmd, wg_x, wg_y, 1);
                    insert_compute_barrier(cmd);
                }
            }

            // --- AO ---
            if (settings.bake_ao && !cs_ao.valid)
            {
                ALOG_WARN("lightmap_baker: AO bake requested but shader failed to compile, skipping");
            }
            if (settings.bake_ao && cs_ao.valid)
            {
                ALOG_INFO("lightmap_baker: dispatching AO");
                uint32_t ao_seed = 91813u;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cs_ao.data.m_pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        cs_ao.data.m_pipeline_layout, 0, 1, &ds_ao, 0, nullptr);
                vkCmdPushConstants(cmd, cs_ao.data.m_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                                   0, sizeof(uint32_t), &ao_seed);
                vkCmdDispatch(cmd, wg_x, wg_y, 1);
                insert_compute_barrier(cmd);
            }

            // --- Denoise ---
            if (!cs_denoise.valid && settings.denoise_iterations > 0)
            {
                ALOG_WARN("lightmap_baker: denoise requested but shader failed to compile, skipping");
            }
            if (cs_denoise.valid)
            {
                for (uint32_t d = 0; d < settings.denoise_iterations; ++d)
                {
                    ALOG_INFO("lightmap_baker: dispatching denoise pass {}/{}",
                              d + 1, settings.denoise_iterations);
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                      cs_denoise.data.m_pipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                            cs_denoise.data.m_pipeline_layout, 0, 1, &ds_denoise,
                                            0, nullptr);
                    vkCmdDispatch(cmd, wg_x, wg_y, 1);
                    insert_compute_barrier(cmd);
                }
            }
        });

    ALOG_INFO("lightmap_baker: GPU dispatch complete");

    // =====================================================================
    // Step 7: Read back results
    // =====================================================================
    size_t lm_byte_size = W * H * 8;  // RGBA16F = 8 bytes per texel
    m_lightmap_data.resize(lm_byte_size);

    {
        // Create readback image (linear tiling, host-visible)
        VkExtent3D extent{W, H, 1};
        auto dst_ci = vk_utils::make_image_create_info(VK_FORMAT_R16G16B16A16_SFLOAT,
                                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);
        dst_ci.tiling = VK_IMAGE_TILING_LINEAR;

        VmaAllocationCreateInfo dst_aci{};
        dst_aci.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        dst_aci.requiredFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        auto readback_img =
            vk_utils::vulkan_image::create(device->get_vma_allocator_provider(), dst_ci, dst_aci);

        device->immediate_submit(
            [&](VkCommandBuffer cmd)
            {
                VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

                // Transition lightmap for transfer source
                vk_utils::make_insert_image_memory_barrier(
                    cmd, img_lightmap->image(), VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, range);

                // Transition readback image
                vk_utils::make_insert_image_memory_barrier(
                    cmd, readback_img.image(), 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, range);

                VkImageCopy copy_region{};
                copy_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy_region.extent = extent;

                vkCmdCopyImage(cmd, img_lightmap->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback_img.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copy_region);

                vk_utils::make_insert_image_memory_barrier(
                    cmd, readback_img.image(), VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT, range);
            });

        auto* mapped = readback_img.map();
        if (mapped)
        {
            memcpy(m_lightmap_data.data(), mapped, lm_byte_size);
            readback_img.unmap();
        }
        else
        {
            ALOG_ERROR("lightmap_baker: failed to map lightmap readback image");
            desc_alloc.cleanup();
            return result;
        }
    }

    // Read back AO
    if (settings.bake_ao)
    {
        size_t ao_byte_size = W * H * 2;  // R16F = 2 bytes per texel
        m_ao_data.resize(ao_byte_size);

        VkExtent3D extent{W, H, 1};
        auto dst_ci =
            vk_utils::make_image_create_info(VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);
        dst_ci.tiling = VK_IMAGE_TILING_LINEAR;

        VmaAllocationCreateInfo dst_aci{};
        dst_aci.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        dst_aci.requiredFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        auto readback_img =
            vk_utils::vulkan_image::create(device->get_vma_allocator_provider(), dst_ci, dst_aci);

        device->immediate_submit(
            [&](VkCommandBuffer cmd)
            {
                VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

                vk_utils::make_insert_image_memory_barrier(
                    cmd, img_ao->image(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, range);

                vk_utils::make_insert_image_memory_barrier(
                    cmd, readback_img.image(), 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, range);

                VkImageCopy copy_region{};
                copy_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy_region.extent = extent;

                vkCmdCopyImage(cmd, img_ao->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback_img.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copy_region);

                vk_utils::make_insert_image_memory_barrier(
                    cmd, readback_img.image(), VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT, range);
            });

        auto* mapped = readback_img.map();
        if (mapped)
        {
            memcpy(m_ao_data.data(), mapped, ao_byte_size);
            readback_img.unmap();
        }
        else
        {
            ALOG_ERROR("lightmap_baker: failed to map AO readback image");
            desc_alloc.cleanup();
            return result;
        }
    }

    // =====================================================================
    // Step 8: Save to disk if output paths are set
    // =====================================================================
    if (!settings.output_lightmap.empty() && !m_lightmap_data.empty())
    {
        if (!vfs::save_file(settings.output_lightmap, m_lightmap_data))
        {
            ALOG_ERROR("lightmap_baker: failed to save lightmap to {}", settings.output_lightmap.str());
        }
        else
        {
            ALOG_INFO("lightmap_baker: saved lightmap to {}", settings.output_lightmap.str());
        }
    }

    if (!settings.output_ao.empty() && !m_ao_data.empty())
    {
        if (!vfs::save_file(settings.output_ao, m_ao_data))
        {
            ALOG_ERROR("lightmap_baker: failed to save AO to {}", settings.output_ao.str());
        }
        else
        {
            ALOG_INFO("lightmap_baker: saved AO to {}", settings.output_ao.str());
        }
    }

    // Save PNG previews (half-float → 8-bit for visual inspection)
    if (settings.output_png)
    {
        auto half_to_float = [](uint16_t h) -> float
        {
            uint32_t sign = (h >> 15) & 1;
            uint32_t exp = (h >> 10) & 0x1F;
            uint32_t mant = h & 0x3FF;
            if (exp == 0)
                return sign ? -0.0f : 0.0f;
            if (exp == 31)
                return sign ? -INFINITY : INFINITY;
            float f = std::ldexp(static_cast<float>(mant | 0x400), static_cast<int>(exp) - 25);
            return sign ? -f : f;
        };

        auto to_u8 = [](float v) -> uint8_t
        {
            return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, v * 255.0f)));
        };

        if (!settings.output_lightmap.empty() && !m_lightmap_data.empty())
        {
            std::vector<uint8_t> rgba8(W * H * 4);
            auto* src = reinterpret_cast<const uint16_t*>(m_lightmap_data.data());
            for (uint32_t i = 0; i < W * H; ++i)
            {
                rgba8[i * 4 + 0] = to_u8(half_to_float(src[i * 4 + 0]));
                rgba8[i * 4 + 1] = to_u8(half_to_float(src[i * 4 + 1]));
                rgba8[i * 4 + 2] = to_u8(half_to_float(src[i * 4 + 2]));
                rgba8[i * 4 + 3] = 255;
            }
            auto png_rid = settings.output_lightmap;
            // Replace .bin with .png in relative path
            auto rel = std::string(png_rid.relative());
            auto dot = rel.rfind('.');
            if (dot != std::string::npos)
                rel = rel.substr(0, dot) + ".png";
            auto png_path = glob::glob_state().getr_vfs().real_path(
                vfs::rid(png_rid.mount_point(), rel));
            if (png_path)
            {
                save_png(APATH(png_path.value()).str(), rgba8.data(), W, H);
                ALOG_INFO("lightmap_baker: saved lightmap PNG preview");
            }
        }

        if (!settings.output_ao.empty() && !m_ao_data.empty())
        {
            std::vector<uint8_t> rgba8(W * H * 4);
            auto* src = reinterpret_cast<const uint16_t*>(m_ao_data.data());
            for (uint32_t i = 0; i < W * H; ++i)
            {
                uint8_t v = to_u8(half_to_float(src[i]));
                rgba8[i * 4 + 0] = v;
                rgba8[i * 4 + 1] = v;
                rgba8[i * 4 + 2] = v;
                rgba8[i * 4 + 3] = 255;
            }
            auto png_rid = settings.output_ao;
            auto rel = std::string(png_rid.relative());
            auto dot = rel.rfind('.');
            if (dot != std::string::npos)
                rel = rel.substr(0, dot) + ".png";
            auto png_path = glob::glob_state().getr_vfs().real_path(
                vfs::rid(png_rid.mount_point(), rel));
            if (png_path)
            {
                save_png(APATH(png_path.value()).str(), rgba8.data(), W, H);
                ALOG_INFO("lightmap_baker: saved AO PNG preview");
            }
        }
    }

    // =====================================================================
    // Step 9: Cleanup (compute_shader_data destructors handle pipeline cleanup)
    // =====================================================================
    desc_alloc.cleanup();

    result.atlas_width = W;
    result.atlas_height = H;

    auto end = std::chrono::high_resolution_clock::now();
    result.bake_time_ms = std::chrono::duration<float, std::milli>(end - start).count();
    result.success = true;

    ALOG_INFO("lightmap_baker: bake complete in {:.1f}ms ({}x{}, {} tris, {} nodes)",
              result.bake_time_ms, W, H, result.total_triangles, result.total_nodes);

    return result;
}

}  // namespace render
}  // namespace kryga

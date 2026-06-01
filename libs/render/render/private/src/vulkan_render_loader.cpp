#include "vulkan_render/vulkan_render_loader.h"

#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/render_system.h"
#include "vulkan_render/vk_pipeline_builder.h"
#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"
#include "vulkan_render/kryga_render.h"
#include "vulkan_render/utils/vulkan_initializers.h"

#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_sampler_data.h"
#include "vulkan_render/shader_reflection_utils.h"

#include <utils/string_utility.h>
#include <utils/file_utils.h>
#include <utils/buffer.h>
#include <utils/process.h>
#include <utils/kryga_log.h>

#include <native/native_window.h>

#include <global_state/global_state.h>

#include <vk_mem_alloc.h>
#include <stb_unofficial/stb.h>

namespace kryga
{

namespace render
{

namespace
{
vk_utils::vulkan_image_sptr
upload_image(int texWidth,
             int texHeight,
             VkFormat image_format,
             vk_utils::vulkan_buffer& stagingBuffer)
{
    auto& device = glob::glob_state().getr_render().device;

    VkExtent3D imageExtent{};
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vk_utils::make_image_create_info(
        image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto new_image = vk_utils::vulkan_image::create(
        device.get_vma_allocator_provider(), dimg_info, dimg_allocinfo, 1);

    // transition image to transfer-receiver
    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            VkImageMemoryBarrier imageBarrier_toTransfer = {};
            imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

            imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier_toTransfer.image = new_image.image();
            imageBarrier_toTransfer.subresourceRange = range;

            imageBarrier_toTransfer.srcAccessMask = 0;
            imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            // barrier the image into the transfer-receive layout
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageBarrier_toTransfer);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = imageExtent;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(cmd,
                                   stagingBuffer.buffer(),
                                   new_image.image(),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &copyRegion);

            VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

            imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            // barrier the image into the shader readable layout
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageBarrier_toReadable);
        });

    return std::make_shared<vk_utils::vulkan_image>(std::move(new_image));
}

}  // namespace

mesh_data*
vulkan_render_loader::create_mesh(const kryga::utils::id& mesh_id,
                                  kryga::utils::buffer_view<gpu::vertex_data> vbv,
                                  kryga::utils::buffer_view<gpu::uint> ibv)
{
    KRG_check(!get_mesh_data(mesh_id), "should never happens");

    auto& device = glob::glob_state().getr_render().device;

    auto md = std::make_shared<mesh_data>(mesh_id);
    md->m_indices_size = (uint32_t)ibv.size();
    md->m_vertices_size = (uint32_t)vbv.size();

    // Tight bounding sphere: AABB-midpoint centroid + max distance from
    // centroid. Computing radius from the local origin (the previous
    // implementation) over-sizes the sphere whenever vertices are offset
    // from origin — common for instanced or sub-mesh geometry.
    {
        glm::vec3 vmin{std::numeric_limits<float>::max()};
        glm::vec3 vmax{std::numeric_limits<float>::lowest()};
        for (uint32_t i = 0; i < vbv.size(); ++i)
        {
            const auto& pos = vbv.at(i).position;
            vmin = glm::min(vmin, pos);
            vmax = glm::max(vmax, pos);
        }
        glm::vec3 centroid = (vmin + vmax) * 0.5f;

        float max_dist_sq = 0.0f;
        for (uint32_t i = 0; i < vbv.size(); ++i)
        {
            glm::vec3 d = vbv.at(i).position - centroid;
            max_dist_sq = std::max(max_dist_sq, glm::dot(d, d));
        }
        md->m_local_centroid = centroid;
        md->m_bounding_radius = std::sqrt(max_dist_sq);
    }

    const uint32_t vertex_buffer_size = (uint32_t)vbv.size_bytes();
    const uint32_t index_buffer_size = (uint32_t)ibv.size_bytes();

    const uint32_t buffer_size = vertex_buffer_size + index_buffer_size;

    // allocate vertex buffer
    VkBufferCreateInfo staging_buffer_ci = {};
    staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_ci.pNext = nullptr;
    // this is the total size, in bytes, of the buffer we are allocating
    staging_buffer_ci.size = buffer_size;
    // this buffer is going to be used as a Vertex Buffer
    staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // let the VMA library know that this data should be writeable by CPU, but also readable by
    // GPU
    VmaAllocationCreateInfo vma_alloc_ci = {};
    vma_alloc_ci.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    auto staging_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_alloc_ci);
    // copy vertex data

    staging_buffer.begin();

    staging_buffer.upload_data(vbv.data(), vertex_buffer_size, false);
    staging_buffer.upload_data(ibv.data(), index_buffer_size, false);

    staging_buffer.end();

    // allocate vertex buffer
    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.pNext = nullptr;
    // this is the total size, in bytes, of the buffer we are allocating
    vertex_buffer_ci.size = vertex_buffer_size;
    // this buffer is going to be used as a Vertex Buffer
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // let the VMA library know that this data should be gpu native
    vma_alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    md->m_vertex_buffer = vk_utils::vulkan_buffer::create(vertex_buffer_ci, vma_alloc_ci);
    // allocate the buffer
    if (index_buffer_size > 0)
    {
        // allocate index buffer
        VkBufferCreateInfo index_buffer_ci = {};
        index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        index_buffer_ci.pNext = nullptr;
        // this is the total size, in bytes, of the buffer we are allocating
        index_buffer_ci.size = index_buffer_size;
        // this buffer is going to be used as a index Buffer
        index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        //         // allocate the buffer
        md->m_index_buffer = vk_utils::vulkan_buffer::create(index_buffer_ci, vma_alloc_ci);
    }

    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy copy;
            copy.dstOffset = 0;
            copy.srcOffset = 0;
            copy.size = vertex_buffer_size;
            vkCmdCopyBuffer(cmd, staging_buffer.buffer(), md->m_vertex_buffer.buffer(), 1, &copy);

            if (index_buffer_size > 0)
            {
                copy.dstOffset = 0;
                copy.srcOffset = vertex_buffer_size;
                copy.size = index_buffer_size;
                vkCmdCopyBuffer(
                    cmd, staging_buffer.buffer(), md->m_index_buffer.buffer(), 1, &copy);
            }
        });

    m_meshes_cache[mesh_id] = md;

    return md.get();
}

mesh_data*
vulkan_render_loader::create_skinned_mesh(const kryga::utils::id& mesh_id,
                                          kryga::utils::buffer_view<gpu::skinned_vertex_data> vbv,
                                          kryga::utils::buffer_view<gpu::uint> ibv)
{
    KRG_check(!get_mesh_data(mesh_id), "should never happens");

    auto& device = glob::glob_state().getr_render().device;

    auto md = std::make_shared<mesh_data>(mesh_id);
    md->m_indices_size = (uint32_t)ibv.size();
    md->m_vertices_size = (uint32_t)vbv.size();
    md->m_is_skinned = true;

    // Tight bounding sphere — see create_mesh() comment.
    {
        glm::vec3 vmin{std::numeric_limits<float>::max()};
        glm::vec3 vmax{std::numeric_limits<float>::lowest()};
        for (uint32_t i = 0; i < vbv.size(); ++i)
        {
            const auto& pos = vbv.at(i).position;
            vmin = glm::min(vmin, pos);
            vmax = glm::max(vmax, pos);
        }
        glm::vec3 centroid = (vmin + vmax) * 0.5f;

        float max_dist_sq = 0.0f;
        for (uint32_t i = 0; i < vbv.size(); ++i)
        {
            glm::vec3 d = vbv.at(i).position - centroid;
            max_dist_sq = std::max(max_dist_sq, glm::dot(d, d));
        }
        md->m_local_centroid = centroid;
        md->m_bounding_radius = std::sqrt(max_dist_sq);
    }

    const uint32_t vertex_buffer_size = (uint32_t)vbv.size_bytes();
    const uint32_t index_buffer_size = (uint32_t)ibv.size_bytes();
    const uint32_t buffer_size = vertex_buffer_size + index_buffer_size;

    // Staging buffer
    VkBufferCreateInfo staging_buffer_ci = {};
    staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_ci.pNext = nullptr;
    staging_buffer_ci.size = buffer_size;
    staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo vma_alloc_ci = {};
    vma_alloc_ci.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    auto staging_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_alloc_ci);

    staging_buffer.begin();
    staging_buffer.upload_data(vbv.data(), vertex_buffer_size, false);
    staging_buffer.upload_data(ibv.data(), index_buffer_size, false);
    staging_buffer.end();

    // Vertex buffer
    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.pNext = nullptr;
    vertex_buffer_ci.size = vertex_buffer_size;
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vma_alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    md->m_vertex_buffer = vk_utils::vulkan_buffer::create(vertex_buffer_ci, vma_alloc_ci);

    // Index buffer
    if (index_buffer_size > 0)
    {
        VkBufferCreateInfo index_buffer_ci = {};
        index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        index_buffer_ci.pNext = nullptr;
        index_buffer_ci.size = index_buffer_size;
        index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        md->m_index_buffer = vk_utils::vulkan_buffer::create(index_buffer_ci, vma_alloc_ci);
    }

    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy copy;
            copy.dstOffset = 0;
            copy.srcOffset = 0;
            copy.size = vertex_buffer_size;
            vkCmdCopyBuffer(cmd, staging_buffer.buffer(), md->m_vertex_buffer.buffer(), 1, &copy);

            if (index_buffer_size > 0)
            {
                copy.dstOffset = 0;
                copy.srcOffset = vertex_buffer_size;
                copy.size = index_buffer_size;
                vkCmdCopyBuffer(
                    cmd, staging_buffer.buffer(), md->m_index_buffer.buffer(), 1, &copy);
            }
        });

    m_meshes_cache[mesh_id] = md;
    return md.get();
}

texture_data*
vulkan_render_loader::create_texture(const kryga::utils::id& texture_id,
                                     const kryga::utils::buffer& base_color,
                                     uint32_t w,
                                     uint32_t h)
{
    KRG_check(!get_texture_data(texture_id), "should never happens");

    auto& device = glob::glob_state().getr_render().device;
    auto& cache = glob::glob_state().getr_render().renderer.get_cache();

    // Allocate texture in render cache - slot becomes bindless index
    auto* td = cache.textures.alloc(texture_id);
    if (!td)
    {
        ALOG_ERROR("Failed to allocate texture in cache");
        return nullptr;
    }

    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    auto staging_buffer = device.create_buffer(
        base_color.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(device.allocator(), staging_buffer.allocation(), &data);
    memcpy(data, base_color.data(), (size_t)base_color.size());
    vmaUnmapMemory(device.allocator(), staging_buffer.allocation());

    td->image = upload_image(w, h, image_format, staging_buffer);

    VkImageViewCreateInfo image_info = vk_utils::make_imageview_create_info(
        VK_FORMAT_R8G8B8A8_UNORM, td->image->image(), VK_IMAGE_ASPECT_COLOR_BIT);
    image_info.subresourceRange.levelCount = td->image->get_mip_levels();

    td->image_view = vk_utils::vulkan_image_view::create_shared(image_info);

    // Mark texture dirty for bindless descriptor update
    glob::glob_state().getr_render().renderer.stage_update_texture(td);

    m_textures_cache[texture_id] = td;

    return td;
}

texture_data*
vulkan_render_loader::create_texture(const kryga::utils::id& texture_id,
                                     const kryga::utils::buffer& data,
                                     uint32_t w,
                                     uint32_t h,
                                     VkFormat vk_format,
                                     texture_format fmt)
{
    KRG_check(!get_texture_data(texture_id), "should never happens");

    auto& device = glob::glob_state().getr_render().device;
    auto& cache = glob::glob_state().getr_render().renderer.get_cache();

    auto* td = cache.textures.alloc(texture_id);
    if (!td)
    {
        ALOG_ERROR("Failed to allocate texture in cache");
        return nullptr;
    }

    auto staging_buffer = device.create_buffer(
        data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* mapped = nullptr;
    vmaMapMemory(device.allocator(), staging_buffer.allocation(), &mapped);
    memcpy(mapped, data.data(), (size_t)data.size());
    vmaUnmapMemory(device.allocator(), staging_buffer.allocation());

    td->image = upload_image(w, h, vk_format, staging_buffer);
    td->format = fmt;

    VkImageViewCreateInfo image_info = vk_utils::make_imageview_create_info(
        vk_format, td->image->image(), VK_IMAGE_ASPECT_COLOR_BIT);
    image_info.subresourceRange.levelCount = td->image->get_mip_levels();

    td->image_view = vk_utils::vulkan_image_view::create_shared(image_info);

    glob::glob_state().getr_render().renderer.stage_update_texture(td);

    m_textures_cache[texture_id] = td;

    return td;
}

texture_data*
vulkan_render_loader::update_or_create_texture(const kryga::utils::id& texture_id,
                                               const kryga::utils::buffer& data,
                                               uint32_t w,
                                               uint32_t h,
                                               VkFormat vk_format,
                                               texture_format fmt)
{
    auto* existing = get_texture_data(texture_id);
    if (!existing)
    {
        return create_texture(texture_id, data, w, h, vk_format, fmt);
    }

    // Replace image data in-place — same bindless slot, safe for in-flight frames.
    // Old image/view are ref-counted (shared_ptr) and will be destroyed when
    // all in-flight command buffers finish.
    auto& device = glob::glob_state().getr_render().device;

    auto staging_buffer = device.create_buffer(
        data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* mapped = nullptr;
    vmaMapMemory(device.allocator(), staging_buffer.allocation(), &mapped);
    memcpy(mapped, data.data(), (size_t)data.size());
    vmaUnmapMemory(device.allocator(), staging_buffer.allocation());

    existing->image = upload_image(w, h, vk_format, staging_buffer);
    existing->format = fmt;

    VkImageViewCreateInfo image_info = vk_utils::make_imageview_create_info(
        vk_format, existing->image->image(), VK_IMAGE_ASPECT_COLOR_BIT);
    image_info.subresourceRange.levelCount = existing->image->get_mip_levels();

    existing->image_view = vk_utils::vulkan_image_view::create_shared(image_info);

    // Re-register in bindless set with the new image view (same slot)
    glob::glob_state().getr_render().renderer.stage_update_texture(existing);

    return existing;
}

texture_data*
vulkan_render_loader::create_texture(const kryga::utils::id& texture_id,
                                     kryga::render::vk_utils::vulkan_image_sptr image,
                                     kryga::render::vk_utils::vulkan_image_view_sptr view)
{
    KRG_check(!get_texture_data(texture_id), "should never happens");

    auto& cache = glob::glob_state().getr_render().renderer.get_cache();

    // Allocate texture in render cache - slot becomes bindless index
    auto* td = cache.textures.alloc(texture_id);
    if (!td)
    {
        ALOG_ERROR("Failed to allocate texture in cache");
        return nullptr;
    }

    td->image = image;
    td->image_view = view;

    // Mark texture dirty for bindless descriptor update
    glob::glob_state().getr_render().renderer.stage_update_texture(td);

    m_textures_cache[texture_id] = td;

    return td;
}

void
vulkan_render_loader::destroy_texture_data(const kryga::utils::id& id)
{
    auto itr = m_textures_cache.find(id);
    if (itr != m_textures_cache.end())
    {
        // Release from cache
        auto& cache = glob::glob_state().getr_render().renderer.get_cache();
        cache.textures.release(itr->second);
        m_textures_cache.erase(itr);
    }
}

bool
vulkan_render_loader::update_object(vulkan_render_data& obj_data,
                                    material_data& mat_data,
                                    mesh_data& mesh_data,
                                    const glm::mat4& model_matrix,
                                    const glm::mat4& normal_matrix,
                                    const glm::vec3& obj_pos)
{
    obj_data.material = &mat_data;
    obj_data.mesh = &mesh_data;

    obj_data.gpu_data.model = model_matrix;
    obj_data.gpu_data.normal = normal_matrix;
    obj_data.gpu_data.obj_pos = obj_pos;

    // Frustum-cull sphere: world-space centroid + scale-aware radius.
    // Conservative for non-uniform scale (uses max axis), but never
    // under-culls. Callers (cmd handlers) may overwrite these with
    // explicitly-supplied values; the defaults here are valid for direct
    // callers (tests, ad-hoc code).
    obj_data.gpu_data.bounding_sphere_center =
        glm::vec3(model_matrix * glm::vec4(mesh_data.m_local_centroid, 1.0f));
    const float sx = glm::length(glm::vec3(model_matrix[0]));
    const float sy = glm::length(glm::vec3(model_matrix[1]));
    const float sz = glm::length(glm::vec3(model_matrix[2]));
    const float max_scale = std::max({sx, sy, sz});
    obj_data.gpu_data.bounding_radius = mesh_data.m_bounding_radius * max_scale;

    // Set material_id for per-object material lookup in shaders
    obj_data.gpu_data.material_id = mat_data.gpu_idx();

    // Default: no light probe assigned (dynamic objects will be assigned per-frame)
    obj_data.gpu_data.probe_index = 0xFFFFFFFFu;

    // Default: identity lightmap transform (no atlas remap), no lightmap texture
    obj_data.gpu_data.lightmap_scale = glm::vec2(1.0f, 1.0f);
    obj_data.gpu_data.lightmap_offset = glm::vec2(0.0f, 0.0f);
    obj_data.gpu_data.lightmap_texture_index = 0xFFFFFFFFu;

    return true;
}

void
vulkan_render_loader::destroy_mesh_data(const kryga::utils::id& id)
{
    auto itr = m_meshes_cache.find(id);
    if (itr != m_meshes_cache.end())
    {
        // shedule_to_deltete_t(std::move(itr->second));
        m_meshes_cache.erase(itr);
    }
}

material_data*
vulkan_render_loader::create_material(const kryga::utils::id& id,
                                      const kryga::utils::id& type_id,
                                      std::vector<texture_sampler_data>& samples,
                                      shader_effect_data& se_data,
                                      const kryga::utils::dynobj& gpu_params)
{
    KRG_check(!get_material_data(id), "Shouldn't exist");

    auto mat_data = std::make_shared<material_data>(id, type_id);

    // Note: Layout validation removed - compile-time generated GPU structs
    // are guaranteed to match shader MaterialData layout by construction.
    // The gpu_params is now a raw buffer without dynobj layout metadata.

    mat_data->set_shader_effect(&se_data);
    mat_data->set_texture_samples(samples);

    // Set bindless texture indices from texture slots
    ALOG_INFO(
        "create_material: {} has {} texture samples", mat_data->get_id().cstr(), samples.size());
    for (const auto& sample : samples)
    {
        if (sample.texture)
        {
            // Texture already has bindless index from its slot in render_cache
            uint32_t bindless_idx = sample.texture->get_bindless_index();
            mat_data->set_bindless_texture_index(sample.slot, bindless_idx);
            ALOG_INFO("  Set slot {} to bindless index {}", sample.slot, bindless_idx);
        }
    }

    // All shaders sample textures through the global bindless set (set 2) by
    // index — there is no per-material descriptor set. (The old legacy set was
    // created here and bound at set 0/3; it's gone now that the UI shaders are
    // bindless too, which also removes the create-on-main descriptor allocation
    // and its leak.)

    mat_data->set_gpu_data(gpu_params);

    m_materials_cache[id] = mat_data;

    return mat_data.get();
}

sampler_data*
vulkan_render_loader::create_sampler(const kryga::utils::id& id, VkBorderColor color)
{
    KRG_check(!get_sampler_data(id), "Shouldn't exist");

    auto& device = glob::glob_state().getr_render().device;

    VkSamplerCreateInfo sampler_ci = vk_utils::make_sampler_create_info(VK_FILTER_LINEAR);

    sampler_ci.maxLod = 30.f;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.borderColor = color;

    auto data = std::make_shared<sampler_data>(id);

    vkCreateSampler(device.vk_device(), &sampler_ci, nullptr, &data->m_sampler);

    m_samplers_cache[id] = data;

    return data.get();
}

void
vulkan_render_loader::destroy_sampler_data(const kryga::utils::id& id)
{
    auto itr = m_samplers_cache.find(id);
    if (itr != m_samplers_cache.end())
    {
        // shedule_to_deltete_t(std::move(itr->second));
        m_samplers_cache.erase(itr);
    }
}

bool
vulkan_render_loader::update_material(material_data& mat_data,
                                      std::vector<texture_sampler_data>& samples,
                                      shader_effect_data& se_data,
                                      const kryga::utils::dynobj& gpu_params)
{
    mat_data.set_shader_effect(&se_data);
    mat_data.set_texture_samples(samples);
    mat_data.set_gpu_data(gpu_params);

    for (const auto& sample : samples)
    {
        if (sample.texture)
        {
            mat_data.set_bindless_texture_index(sample.slot, sample.texture->get_bindless_index());
        }
    }

    return true;
}

void
vulkan_render_loader::destroy_material_data(const kryga::utils::id& id)
{
    auto itr = m_materials_cache.find(id);
    if (itr != m_materials_cache.end())
    {
        // No per-material descriptor set to free — textures are bindless.
        m_materials_cache.erase(itr);
    }
}

void
vulkan_render_loader::create_font(const kryga::utils::id& id, ImFont* font)
{
    m_fonts_cache[id] = font;
}

void
vulkan_render_loader::destroy_render_pass(const kryga::utils::id& id)
{
    auto itr = m_render_passes.find(id);
    if (itr != m_render_passes.end())
    {
        m_render_passes.erase(itr);
    }
}

void
vulkan_render_loader::clear_caches()
{
    ALOG_INFO(
        "clear_caches: meshes={} textures={} materials={} shaders={} "
        "samplers={} render_passes={} materials_index={}",
        m_meshes_cache.size(),
        m_textures_cache.size(),
        m_materials_cache.size(),
        m_shaders_cache.size(),
        m_samplers_cache.size(),
        m_render_passes.size(),
        m_materials_index.size());

    for (auto& [id, td] : m_textures_cache)
    {
        // Texture data is owned by render_cache.textures (combined_pool),
        // which renderer.deinit() clears BEFORE this method runs. The td*
        // here is dangling — log only the id (string-pooled, safe).
        ALOG_INFO("  texture id={}", id.cstr());
    }
    for (auto& [id, rp] : m_render_passes)
    {
        ALOG_INFO("  render_pass id={}", id.cstr());
    }

    m_meshes_cache.clear();
    m_textures_cache.clear();
    m_materials_cache.clear();
    m_shaders_cache.clear();
    m_samplers_cache.clear();
    m_render_passes.clear();
    m_materials_index.clear();

}  // namespace render
}  // namespace render

}  // namespace kryga
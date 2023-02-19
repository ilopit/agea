#include "vulkan_render/vulkan_render_loader.h"

#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vk_pipeline_builder.h"
#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"
#include "vulkan_render/utils/vulkan_initializers.h"

#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_sampler_data.h"

#include <utils/string_utility.h>
#include <utils/file_utils.h>
#include <utils/buffer.h>
#include <utils/process.h>
#include <utils/agea_log.h>

#include <native/native_window.h>

#include <resource_locator/resource_locator.h>

#include <serialization/serialization.h>

#include <vk_mem_alloc.h>
#include <stb_unofficial/stb.h>

namespace agea
{

glob::vulkan_render_loader::type glob::vulkan_render_loader::type::s_instance;

namespace render
{

namespace
{
vk_utils::vulkan_image
upload_image(int texWidth,
             int texHeight,
             VkFormat image_format,
             vk_utils::vulkan_buffer& stagingBuffer)
{
    auto device = glob::render_device::get();

    VkExtent3D imageExtent{};
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vk_utils::make_image_create_info(
        image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto new_image = vk_utils::vulkan_image::create(device->get_vma_allocator_provider(), dimg_info,
                                                    dimg_allocinfo, 1);

    // transition image to transfer-receiver
    device->immediate_submit(
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
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
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
            vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer(), new_image.image(),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

            imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            // barrier the image into the shader readable layout
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                                 1, &imageBarrier_toReadable);
        });

    return new_image;
}
bool
load_image_from_file_r(const std::string& file, vk_utils::vulkan_image& outImage)
{
    auto device = glob::render_device::get();

    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load(file.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    void* pixel_ptr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4ULL;

    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    auto stagingBuffer = device->create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    vmaMapMemory(device->allocator(), stagingBuffer.allocation(), &data);

    memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));

    vmaUnmapMemory(device->allocator(), stagingBuffer.allocation());

    stbi_image_free(pixels);

    outImage = upload_image(texWidth, texHeight, image_format, stagingBuffer);

    return true;
}

bool
load_1x1_image_from_color(const std::string& color, vk_utils::vulkan_image& outImage)
{
    auto device = glob::render_device::get();

    std::array<uint8_t, 4> color_data{};

    string_utils::convert_hext_string_to_bytes(color.size() - 1, (&color.front()) + 1,
                                               color_data.data());

    VkDeviceSize size = 4ULL;

    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    auto stagingBuffer =
        device->create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(device->allocator(), stagingBuffer.allocation(), &data);

    memcpy(data, color_data.data(), static_cast<size_t>(size));

    vmaUnmapMemory(device->allocator(), stagingBuffer.allocation());

    outImage = upload_image(1, 1, image_format, stagingBuffer);

    return true;
}

}  // namespace

mesh_data*
vulkan_render_loader::create_mesh(const agea::utils::id& mesh_id,
                                  agea::utils::buffer_view<render::gpu_vertex_data> vbv,
                                  agea::utils::buffer_view<render::gpu_index_data> ibv)
{
    AGEA_check(!get_mesh_data(mesh_id), "should never happens");

    auto device = glob::render_device::get();

    auto md = std::make_shared<mesh_data>(mesh_id);
    md->m_indices_size = (uint32_t)ibv.size();
    md->m_vertices_size = (uint32_t)vbv.size();

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

    auto staging_buffer = vk_utils::vulkan_buffer::create(device->get_vma_allocator_provider(),
                                                          staging_buffer_ci, vma_alloc_ci);
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

    md->m_vertex_buffer = vk_utils::vulkan_buffer::create(device->get_vma_allocator_provider(),
                                                          vertex_buffer_ci, vma_alloc_ci);
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
        md->m_index_buffer = vk_utils::vulkan_buffer::create(device->get_vma_allocator_provider(),
                                                             index_buffer_ci, vma_alloc_ci);
    }

    device->immediate_submit(
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
                vkCmdCopyBuffer(cmd, staging_buffer.buffer(), md->m_index_buffer.buffer(), 1,
                                &copy);
            }
        });

    m_meshes_cache[mesh_id] = md;

    return md.get();
}

texture_data*
vulkan_render_loader::create_texture(const agea::utils::id& texture_id,
                                     const agea::utils::buffer& base_color,
                                     uint32_t w,
                                     uint32_t h)
{
    AGEA_check(!get_texture_data(texture_id), "should never happens");

    auto device = glob::render_device::get();

    auto td = std::make_shared<texture_data>(texture_id, device->get_vk_device_provider());

    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    auto staging_buffer = device->create_buffer(base_color.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(device->allocator(), staging_buffer.allocation(), &data);
    memcpy(data, base_color.data(), (size_t)base_color.size());
    vmaUnmapMemory(device->allocator(), staging_buffer.allocation());

    td->image = upload_image(w, h, image_format, staging_buffer);

    VkImageViewCreateInfo image_info = vk_utils::make_imageview_create_info(
        VK_FORMAT_R8G8B8A8_UNORM, td->image.image(), VK_IMAGE_ASPECT_COLOR_BIT);
    image_info.subresourceRange.levelCount = td->image.get_mip_levels();
    vkCreateImageView(device->vk_device(), &image_info, nullptr, &td->image_view);

    m_textures_cache[texture_id] = td;

    return td.get();
}

object_data*
vulkan_render_loader::create_object(const agea::utils::id& id,
                                    material_data& mat_data,
                                    mesh_data& mesh_data,
                                    const glm::mat4& model_matrix,
                                    const glm::mat4& normal_matrix,
                                    const glm::vec3& obj_pos)
{
    AGEA_check(!get_material_data(id), "Shoudn't exist");

    auto data = std::make_shared<object_data>(id, generate_new_object_index());

    update_object(*data, mat_data, mesh_data, model_matrix, normal_matrix, obj_pos);

    m_objects_cache[id] = data;

    return data.get();
}

bool
vulkan_render_loader::update_object(object_data& obj_data,
                                    material_data& mat_data,
                                    mesh_data& mesh_data,
                                    const glm::mat4& model_matrix,
                                    const glm::mat4& normal_matrix,
                                    const glm::vec3& obj_pos)
{
    obj_data.material = &mat_data;
    obj_data.mesh = &mesh_data;

    obj_data.gpu_data.model_matrix = model_matrix;
    obj_data.gpu_data.normal_matrix = normal_matrix;
    obj_data.gpu_data.obj_pos = obj_pos;

    return true;
}

material_data*
vulkan_render_loader::create_material(const agea::utils::id& id,
                                      const agea::utils::id& type_id,
                                      std::vector<texture_sampler_data>& samples,
                                      shader_effect_data& se_data,
                                      const agea::utils::dynamic_object& gpu_params)
{
    AGEA_check(!get_material_data(id), "Shouldn't exist");

    auto device = glob::render_device::get();

    auto mt_idx = generate_mt_idx(type_id);

    auto mat_data = std::make_shared<material_data>(id, type_id, mt_idx);

    mat_data->effect = &se_data;
    mat_data->texture_samples = samples;

    VkDescriptorImageInfo image_buffer_info{};

    image_buffer_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (auto& ts : mat_data->texture_samples)
    {
        auto sampler = get_sampler_data(AID("default"));
        image_buffer_info.sampler = sampler->m_sampler;
        image_buffer_info.imageView = ts.texture->image_view;

        vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                            device->descriptor_allocator())
            .bind_image(ts.slot, &image_buffer_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(ts.descriptor_set);
    }

    mat_data->gpu_data = gpu_params;

    m_materials_cache[id] = mat_data;

    return mat_data.get();
}

sampler_data*
vulkan_render_loader::create_sampler(const agea::utils::id& id, VkBorderColor color)
{
    AGEA_check(!get_sampler_data(id), "Shouldn't exist");

    auto device = glob::render_device::get();

    VkSamplerCreateInfo sampler_ci = vk_utils::make_sampler_create_info(VK_FILTER_LINEAR);

    sampler_ci.maxLod = 30.f;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.borderColor = color;

    auto data = std::make_shared<sampler_data>(id, device->get_vk_device_provider());

    vkCreateSampler(glob::render_device::get()->vk_device(), &sampler_ci, nullptr,
                    &data->m_sampler);

    m_samplers_cache[id] = data;

    return data.get();
}

bool
vulkan_render_loader::update_material(material_data& mat_data,
                                      std::vector<texture_sampler_data>& samples,
                                      shader_effect_data& se_data,
                                      const agea::utils::dynamic_object& gpu_params)
{
    mat_data.effect = &se_data;
    mat_data.texture_samples = samples;
    mat_data.gpu_data = gpu_params;

    return true;
}

shader_effect_data*
vulkan_render_loader::create_shader_effect(const agea::utils::id& id,
                                           const shader_effect_create_info& info)
{
    AGEA_check(!get_shader_data(id), "should never happens");

    auto device = glob::render_device::get();
    auto effect = std::make_shared<shader_effect_data>(id, device->get_vk_device_provider());

    if (!vulkan_shader_loader::create_shader_effect(*effect, info))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }
    m_shaders_effects_cache[id] = effect;

    return effect.get();
}

bool
vulkan_render_loader::update_shader_effect(shader_effect_data& se_data,
                                           const shader_effect_create_info& info)
{
    AGEA_check(get_shader_data(se_data.get_id()), "should never happens");

    if (!info.frag_buffer->has_file_updated() && !info.vert_buffer->has_file_updated() &&
        info.is_wire == se_data.m_is_wire && info.enable_alpha == se_data.m_enable_alpha)
    {
        ALOG_TRACE("No need to re-create shader effect");
    }

    std::shared_ptr<render::shader_effect_data> old_se_data;

    if (!vulkan_shader_loader::update_shader_effect(se_data, info, old_se_data))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    resource_deleter rd = [old_se_data]() mutable
    {
        ALOG_INFO("Shader effect deleted");
        old_se_data.reset();
    };

    shedule_to_deltete(rd);

    return true;
}

void
vulkan_render_loader::clear_caches()
{
    while (!m_ddq.empty())
    {
        m_ddq.top().deleter();
        m_ddq.pop();
    }

    m_meshes_cache.clear();
    m_textures_cache.clear();
    m_materials_cache.clear();
    m_shaders_cache.clear();
    m_shaders_effects_cache.clear();
    m_objects_cache.clear();
    m_samplers_cache.clear();
}

void
vulkan_render_loader::shedule_to_deltete(resource_deleter d)
{
    auto size = glob::render_device::getr().get_current_frame_number() + FRAMES_IN_FLYIGNT * 2;

    m_ddq.emplace(size, d);
}

void
vulkan_render_loader::delete_sheduled_actions()
{
    if (m_ddq.empty())
    {
        return;
    }

    auto& top = m_ddq.top();

    auto device = glob::render_device::get();
    auto current_frame = device->get_current_frame_number();
    if (top.frame_to_delete <= current_frame)
    {
        top.deleter();
        m_ddq.pop();
    }
}

}  // namespace render
}  // namespace agea

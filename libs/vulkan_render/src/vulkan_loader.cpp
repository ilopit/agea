#include "vulkan_render/vulkan_loader.h"

#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/render_device.h"
#include "vulkan_render/vk_pipeline_builder.h"
#include "vulkan_render/vk_transit.h"
#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"

#include <vulkan_render_types/vulkan_mesh_data.h>
#include <vulkan_render_types/vulkan_texture_data.h>
#include <vulkan_render_types/vulkan_material_data.h>
#include <vulkan_render_types/vulkan_render_data.h>
#include <vulkan_render_types/vulkan_gpu_types.h>
#include <vulkan_render_types/vulkan_initializers.h>
#include <vulkan_render_types/vulkan_shader_data.h>
#include <vulkan_render_types/vulkan_shader_effect_data.h>

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
allocated_image
upload_image(int texWidth, int texHeight, VkFormat image_format, allocated_buffer& stagingBuffer)
{
    auto device = glob::render_device::get();

    VkExtent3D imageExtent{};
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = utils::image_create_info(
        image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    allocated_image newImage =
        allocated_image::create(device->get_vma_allocator_provider(), dimg_info, dimg_allocinfo, 1);

    // transition image to transfer-receiver
    device->immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkImageSubresourceRange range;
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            VkImageMemoryBarrier imageBarrier_toTransfer = {};
            imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

            imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier_toTransfer.image = newImage.image();
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
            vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer(), newImage.image(),
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

    return newImage;
}
bool
load_image_from_file_r(const std::string& file, allocated_image& outImage)
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

    allocated_buffer stagingBuffer = device->create_buffer(
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    vmaMapMemory(device->allocator(), stagingBuffer.allocation(), &data);

    memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));

    vmaUnmapMemory(device->allocator(), stagingBuffer.allocation());

    stbi_image_free(pixels);

    outImage = upload_image(texWidth, texHeight, image_format, stagingBuffer);

    return true;
}

bool
load_material_config(const agea::utils::path& path, agea::render::gpu_material_data& data)
{
    agea::serialization::conteiner c;
    if (!serialization::read_container(path, c))
    {
        return false;
    }

    data.albedo = c["albedo"].as<float>();
    data.gamma = c["gamma"].as<float>();
    data.metallic = c["metallic"].as<float>();
    data.roughness = c["roughness"].as<float>();

    return true;
}

bool
load_1x1_image_from_color(const std::string& color, allocated_image& outImage)
{
    auto device = glob::render_device::get();

    std::array<uint8_t, 4> color_data{};

    string_utils::convert_hext_string_to_bytes(color.size() - 1, (&color.front()) + 1,
                                               color_data.data());

    VkDeviceSize size = 4ULL;

    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    allocated_buffer stagingBuffer =
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
vulkan_loader::create_mesh(const agea::utils::id& mesh_id,
                           agea::utils::buffer_view<render::gpu_vertex_data> vbv,
                           agea::utils::buffer_view<render::gpu_index_data> ibv)
{
    auto device = glob::render_device::get();

    auto itr = m_meshes_cache.find(mesh_id);

    if (itr != m_meshes_cache.end())
    {
        return itr->second.get();
    }

    auto md = std::make_shared<mesh_data>(mesh_id);
    md->m_indices_size = (uint32_t)ibv.size();
    md->m_vertices_size = (uint32_t)vbv.size();

    const uint32_t vertex_buffer_size = (uint32_t)vbv.size_bytes();
    const uint32_t index_buffer_size = (uint32_t)ibv.size_bytes();

    const uint32_t bufferSize = vertex_buffer_size + index_buffer_size;

    // allocate vertex buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.pNext = nullptr;
    // this is the total size, in bytes, of the buffer we are allocating
    stagingBufferInfo.size = bufferSize;
    // this buffer is going to be used as a Vertex Buffer
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // let the VMA library know that this data should be writeable by CPU, but also readable by
    // GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    render::transit_buffer stagingBuffer(allocated_buffer::create(
        device->get_vma_allocator_provider(), stagingBufferInfo, vmaallocInfo));
    // copy vertex data

    stagingBuffer.begin();

    stagingBuffer.upload_data(vbv.data(), vertex_buffer_size, false);
    stagingBuffer.upload_data(ibv.data(), index_buffer_size, false);

    stagingBuffer.end();

    // allocate vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.pNext = nullptr;
    // this is the total size, in bytes, of the buffer we are allocating
    vertexBufferInfo.size = vertex_buffer_size;
    // this buffer is going to be used as a Vertex Buffer
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // let the VMA library know that this data should be gpu native
    vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    md->m_vertexBuffer = allocated_buffer::create(device->get_vma_allocator_provider(),
                                                  vertexBufferInfo, vmaallocInfo);
    // allocate the buffer
    if (index_buffer_size > 0)
    {
        // allocate index buffer
        VkBufferCreateInfo indexBufferInfo = {};
        indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexBufferInfo.pNext = nullptr;
        // this is the total size, in bytes, of the buffer we are allocating
        indexBufferInfo.size = index_buffer_size;
        // this buffer is going to be used as a index Buffer
        indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        //         // allocate the buffer
        md->m_indexBuffer = allocated_buffer::create(device->get_vma_allocator_provider(),
                                                     indexBufferInfo, vmaallocInfo);
    }

    device->immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy copy;
            copy.dstOffset = 0;
            copy.srcOffset = 0;
            copy.size = vertex_buffer_size;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer(), md->m_vertexBuffer.buffer(), 1, &copy);

            if (index_buffer_size > 0)
            {
                copy.dstOffset = 0;
                copy.srcOffset = vertex_buffer_size;
                copy.size = index_buffer_size;
                vkCmdCopyBuffer(cmd, stagingBuffer.buffer(), md->m_indexBuffer.buffer(), 1, &copy);
            }
        });

    m_meshes_cache[mesh_id] = md;

    return md.get();
}

texture_data*
vulkan_loader::create_texture(const agea::utils::id& texture_id,
                              const agea::utils::buffer& base_color,
                              uint32_t w,
                              uint32_t h)
{
    auto device = glob::render_device::get();

    auto titr = m_textures_cache.find(texture_id);
    if (titr != m_textures_cache.end())
    {
        return titr->second.get();
    }

    auto td = std::make_shared<texture_data>(texture_id, device->get_vk_device_provider());

    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    allocated_buffer stagingBuffer = device->create_buffer(
        base_color.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(device->allocator(), stagingBuffer.allocation(), &data);
    memcpy(data, base_color.data(), (size_t)base_color.size());
    vmaUnmapMemory(device->allocator(), stagingBuffer.allocation());

    td->image = upload_image(w, h, image_format, stagingBuffer);

    VkImageViewCreateInfo imageinfo = utils::imageview_create_info(
        VK_FORMAT_R8G8B8A8_UNORM, td->image.image(), VK_IMAGE_ASPECT_COLOR_BIT);
    imageinfo.subresourceRange.levelCount = td->image.get_mip_levels();
    vkCreateImageView(device->vk_device(), &imageinfo, nullptr, &td->image_view);

    VkDescriptorImageInfo imageBufferInfo{};
    imageBufferInfo.sampler = device->sampler("default");
    imageBufferInfo.imageView = td->image_view;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        device->descriptor_allocator())
        .bind_image(0, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(td->descriptor_set);

    m_textures_cache[texture_id] = td;

    return td.get();
}

object_data*
vulkan_loader::create_object(const agea::utils::id& id,
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
vulkan_loader::update_object(object_data& obj_data,
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
vulkan_loader::create_material(const agea::utils::id& id,
                               const agea::utils::id& type_id,
                               texture_data& t_data,
                               shader_effect_data& se_data,
                               const agea::utils::dynamic_object& gpu_params)
{
    auto device = glob::render_device::get();

    AGEA_check(!get_material_data(id), "Shouldn't exist");

    auto mtt_id = generate_mtt_id(type_id);
    auto mt_idx = generate_mtt_id(type_id);

    auto data = std::make_shared<material_data>(id, mtt_id, mt_idx);

    update_material(*data, t_data, se_data, gpu_params);

    m_materials_cache[id] = data;

    return data.get();
}

bool
vulkan_loader::update_material(material_data& mat_data,
                               texture_data& t_data,
                               shader_effect_data& se_data,
                               const agea::utils::dynamic_object& gpu_params)
{
    mat_data.texture_id = t_data.id();
    mat_data.effect = &se_data;
    mat_data.texture_set = t_data.descriptor_set;
    mat_data.gpu_data = gpu_params;

    return true;
}

shader_effect_data*
vulkan_loader::create_shader_effect(const agea::utils::id& id,
                                    agea::utils::buffer& vert_buffer,
                                    bool is_vert_binary,
                                    agea::utils::buffer& frag_buffer,
                                    bool is_frag_binary,
                                    bool is_wire,
                                    bool enable_alpha,
                                    VkRenderPass render_path)
{
    AGEA_check(!get_shader_data(id), "should never happens");

    auto device = glob::render_device::get();
    auto effect = std::make_shared<shader_effect_data>(id, device->get_vk_device_provider());

    if (!vulkan_shader_loader::create_shader_effect(*effect, vert_buffer, is_vert_binary,
                                                    frag_buffer, is_frag_binary, is_wire,
                                                    enable_alpha, render_path))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }
    m_shaders_effects_cache[id] = effect;

    return effect.get();
}

bool
vulkan_loader::update_shader_effect(shader_effect_data& se_data,
                                    agea::utils::buffer& vert_buffer,
                                    bool is_vert_binary,
                                    agea::utils::buffer& frag_buffer,
                                    bool is_frag_binary,
                                    bool is_wire,
                                    bool enable_alpha,
                                    VkRenderPass render_path)
{
    AGEA_check(get_shader_data(se_data.id()), "should never happens");

    if (!frag_buffer.has_file_updated() && !vert_buffer.has_file_updated() &&
        is_wire == se_data.m_is_wire && enable_alpha == se_data.m_enable_alpha)
    {
        ALOG_TRACE("No need to re-create shader effect");
    }

    std::shared_ptr<render::shader_effect_data> old_se_data;

    if (!vulkan_shader_loader::update_shader_effect(se_data, vert_buffer, is_vert_binary,
                                                    frag_buffer, is_frag_binary, is_wire,
                                                    enable_alpha, render_path, old_se_data))
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
vulkan_loader::clear_caches()
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
}

void
vulkan_loader::shedule_to_deltete(resource_deleter d)
{
    auto size = glob::render_device::getr().get_current_frame_number() + FRAMES_IN_FLYIGNT * 2;

    m_ddq.emplace(size, d);
}

void
vulkan_loader::delete_sheduled_actions()
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

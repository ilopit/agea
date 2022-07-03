#include "vulkan_render/render_loader.h"

#include "vulkan_render/vulkan_mesh_data.h"
#include "vulkan_render/vulkan_texture_data.h"
#include "vulkan_render/vulkan_material_data.h"
#include "vulkan_render/render_device.h"
#include "vulkan_render/data_loaders/vulkan_mesh_data_loader.h"

#include "vk_mem_alloc.h"

#include "core/vk_engine.h"
#include "core/vk_engine.h"
#include "core/vk_initializers.h"
#include "core/fs_locator.h"

#include "utils/string_utility.h"
#include "utils/file_utils.h"

#include "model/rendering/mesh.h"
#include "model/rendering/texture.h"
#include "model/rendering/material.h"
#include "model/package.h"

#include <stb_image.h>

#include <iostream>

namespace agea
{
namespace render
{
namespace
{
struct vertex_f32_pncv
{
    float position[3];
    float normal[3];
    float color[3];
    float uv[2];
};

allocated_image
upload_image(int texWidth, int texHeight, VkFormat image_format, allocated_buffer& stagingBuffer)
{
    auto device = glob::render_device::get();

    VkExtent3D imageExtent;
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vk_init::image_create_info(
        image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    allocated_image newImage;

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    // allocate and create the image
    vmaCreateImage(device->allocator(), &dimg_info, &dimg_allocinfo, &newImage.m_image,
                   &newImage.m_allocation, nullptr);

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
            imageBarrier_toTransfer.image = newImage.m_image;
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
            vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer(), newImage.m_image,
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

    newImage.mipLevels = 1;  // mips.size();
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
loader::load_mesh(model::mesh& mc)
{
    auto device = glob::render_device::get();

    auto itr = m_meshes_cache.find(mc.get_id());

    if (itr != m_meshes_cache.end())
    {
        return itr->second.get();
    }

    auto md = std::make_shared<mesh_data>();
    md->m_id = mc.get_id();

    if (!mc.get_external_path().empty())
    {
        auto path =
            glob::resource_locator::get()->resource(category::assets, mc.get_external_path());
        if (!vulkan_mesh_data_loader::load_from_obj(path, *md))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }
    else
    {
        auto p = mc.get_package();

        auto idx_file = p->get_resource_path(mc.get_indices());
        auto vert_file = p->get_resource_path(mc.get_vertices());

        if (!vulkan_mesh_data_loader::load_from_amsh(idx_file, vert_file, *md))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }

    const size_t vertex_buffer_size = md->m_vertices.size() * sizeof(render::vertex_data);
    const size_t index_buffer_size = md->m_indices.size() * sizeof(uint32_t);
    const size_t bufferSize = vertex_buffer_size + index_buffer_size;
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

    allocated_buffer stagingBuffer;

    // allocate the buffer
    stagingBuffer = allocated_buffer::create(stagingBufferInfo, vmaallocInfo);

    // copy vertex data
    char* data;
    vmaMapMemory(device->allocator(), stagingBuffer.allocation(), (void**)&data);

    memcpy(data, md->m_vertices.data(), vertex_buffer_size);
    memcpy(data + vertex_buffer_size, md->m_indices.data(), index_buffer_size);

    vmaUnmapMemory(device->allocator(), stagingBuffer.allocation());

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

    md->m_vertexBuffer = allocated_buffer::create(vertexBufferInfo, vmaallocInfo);
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
        md->m_indexBuffer = allocated_buffer::create(indexBufferInfo, vmaallocInfo);
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

    m_meshes_cache[mc.get_id()] = md;

    return md.get();
}

bool
loader::create_default_material(VkPipeline pipeline, shader_effect* effect, const core::id& id)
{
    auto it = m_materials_cache.find(id);
    if (it != m_materials_cache.end())
    {
        AGEA_never("RECREATE");
        return it->second.get();
    }

    auto mat = std::make_shared<render::material_data>();
    mat->pipeline = pipeline;
    mat->effect = effect;
    m_materials_cache[id] = mat;

    return mat.get();
}

texture_data*
loader::load_texture(model::texture& t)
{
    auto device = glob::render_device::get();

    auto titr = m_textures_cache.find(t.get_id());
    if (titr != m_textures_cache.end())
    {
        return titr->second.get();
    }

    auto td = std::make_shared<texture_data>();

    if (t.get_base_color().front() == '#')
    {
        if (!load_1x1_image_from_color(t.get_base_color(), td->image))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }
    else
    {
        auto p = t.get_package();

        AGEA_check(p, "Package shoul'd be set");

        auto color_path = p->get_resource_path(t.get_base_color());

        if (!load_image_from_file_r(color_path.str(), td->image))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }

    VkImageViewCreateInfo imageinfo = vk_init::imageview_create_info(
        VK_FORMAT_R8G8B8A8_UNORM, td->image.m_image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageinfo.subresourceRange.levelCount = td->image.mipLevels;
    vkCreateImageView(device->vk_device(), &imageinfo, nullptr, &td->image_view);

    m_textures_cache[t.get_id()] = td;

    return td.get();
}

material_data*
loader::load_material(model::material& d)
{
    if (!d.get_base_texture())
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    auto device = glob::render_device::get();

    auto it = m_materials_cache.find(d.get_id());
    if (it != m_materials_cache.end())
    {
        return it->second.get();
    }

    auto td = std::make_shared<material_data>();
    auto base_effect_id = d.get_base_effect();
    auto& def = m_materials_cache.at(base_effect_id);
    td->id = d.get_id();

    td->pipeline = def->pipeline;
    td->effect = def->effect;

    m_materials_cache[d.get_id()] = td;

    VkDescriptorImageInfo imageBufferInfo{};
    imageBufferInfo.sampler = device->sampler("default");
    imageBufferInfo.imageView = m_textures_cache[d.get_base_texture()->get_id()]->image_view;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        device->descriptor_allocator())
        .bind_image(0, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(td->texture_set);

    return td.get();
}

shader_data*
loader::load_shader(const core::id& path)
{
    auto device = glob::render_device::get();

    auto it = m_shaders_cache.find(path);
    if (it != m_shaders_cache.end())
    {
        ALOG_LAZY_ERROR;
        return it->second.get();
    }

    std::vector<char> buffer;
    auto full_path =
        glob::resource_locator::get()->resource(category::shaders_compiled, path.str());
    if (!utils::file_utils::load_file(full_path, buffer))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = (uint32_t*)buffer.data();

    // check that the creation goes well.
    VkShaderModule module;
    if (vkCreateShaderModule(device->vk_device(), &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    auto sd = std::make_shared<shader_data>(device->vk_device(), module, std::move(buffer));

    m_shaders_cache[path] = sd;

    return sd.get();
}

void
loader::clear_caches()
{
    m_meshes_cache.clear();
    m_textures_cache.clear();
    m_materials_cache.clear();
    m_shaders_cache.clear();
}

}  // namespace render
}  // namespace agea

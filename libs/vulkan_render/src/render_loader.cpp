#include "vulkan_render/render_loader.h"

#include <vulkan_render_types/vulkan_mesh_data.h>
#include <vulkan_render_types/vulkan_texture_data.h>
#include <vulkan_render_types/vulkan_material_data.h>
#include <vulkan_render_types/vulkan_gpu_types.h>

#include "vulkan_render/render_device.h"
#include "vulkan_render/data_loaders/vulkan_mesh_data_loader.h"

#include "vk_mem_alloc.h"

#include "vulkan_render_types/vulkan_initializers.h"

#include "utils/string_utility.h"
#include "utils/file_utils.h"

#include "model/rendering/mesh.h"

#include <stb_unofficial/stb.h>

#include <iostream>
#include <sstream>
#include <spirv_reflect.h>

namespace agea
{
namespace render
{

namespace
{
constexpr uint32_t
fnv1a_32(char const* s, std::size_t count)
{
    return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
}

}  // namespace

uint32_t
hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info)
{
    // we are going to put all the data into a string and then hash the string
    std::stringstream ss;

    ss << info->flags;
    ss << info->bindingCount;

    for (uint32_t i = 0; i < info->bindingCount; i++)
    {
        const VkDescriptorSetLayoutBinding& binding = info->pBindings[i];

        ss << binding.binding;
        ss << binding.descriptorCount;
        ss << binding.descriptorType;
        ss << binding.stageFlags;
    }

    auto str = ss.str();

    return fnv1a_32(str.c_str(), str.length());
}

namespace
{

struct vertex_f32_pncv
{
    float position[3];
    float normal[3];
    float color[3];
    float uv[2];
};

struct DescriptorSetLayoutData
{
    int set_number;
    VkDescriptorSetLayoutCreateInfo create_info;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

allocated_image
upload_image(int texWidth, int texHeight, VkFormat image_format, allocated_buffer& stagingBuffer)
{
    auto device = glob::render_device::get();

    VkExtent3D imageExtent;
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = utils::image_create_info(
        image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    auto allloc_cb = []() { return glob::render_device::get()->allocator(); };

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    allocated_image newImage = allocated_image::create(allloc_cb, dimg_info, dimg_allocinfo, 1);

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

void
reflect_layout(render_device* engine,
               shader_effect& se,
               shader_effect::reflection_overrides* overrides,
               int overrideCount)
{
    std::vector<DescriptorSetLayoutData> set_layouts;

    std::vector<VkPushConstantRange> constant_ranges;

    for (auto& s : se.m_stages)
    {
        SpvReflectShaderModule spvmodule;
        SpvReflectResult result =
            spvReflectCreateShaderModule(s.m_shaderModule->code().size() * sizeof(uint32_t),
                                         s.m_shaderModule->code().data(), &spvmodule);

        uint32_t count = 0;
        result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectDescriptorSet*> sets(count);
        result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, sets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (size_t i_set = 0; i_set < sets.size(); ++i_set)
        {
            const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);

            DescriptorSetLayoutData layout = {};

            layout.bindings.resize(refl_set.binding_count);
            for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding)
            {
                const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
                VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
                layout_binding.binding = refl_binding.binding;
                layout_binding.descriptorType =
                    static_cast<VkDescriptorType>(refl_binding.descriptor_type);

                for (int ov = 0; ov < overrideCount; ov++)
                {
                    if (strcmp(refl_binding.name, overrides[ov].m_name) == 0)
                    {
                        layout_binding.descriptorType = overrides[ov].m_overridenType;
                    }
                }

                layout_binding.descriptorCount = 1;
                for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
                {
                    layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
                }
                layout_binding.stageFlags =
                    static_cast<VkShaderStageFlagBits>(spvmodule.shader_stage);

                shader_effect::reflected_binding reflected;
                reflected.m_binding = layout_binding.binding;
                reflected.m_set = refl_set.set;
                reflected.m_type = layout_binding.descriptorType;

                se.m_bindings[refl_binding.name] = reflected;
            }
            layout.set_number = refl_set.set;
            layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout.create_info.bindingCount = refl_set.binding_count;
            layout.create_info.pBindings = layout.bindings.data();

            set_layouts.push_back(layout);
        }

        // pushconstants

        result = spvReflectEnumeratePushConstantBlocks(&spvmodule, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectBlockVariable*> pconstants(count);
        result = spvReflectEnumeratePushConstantBlocks(&spvmodule, &count, pconstants.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        if (count > 0)
        {
            VkPushConstantRange pcs{};
            pcs.offset = pconstants[0]->offset;
            pcs.size = pconstants[0]->size;
            pcs.stageFlags = s.m_stage;

            constant_ranges.push_back(pcs);
        }

        spvReflectDestroyShaderModule(&spvmodule);
    }

    std::array<DescriptorSetLayoutData, 4> merged_layouts;

    for (int i = 0; i < 4; i++)
    {
        DescriptorSetLayoutData& ly = merged_layouts[i];

        ly.set_number = i;

        ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        std::unordered_map<int, VkDescriptorSetLayoutBinding> binds;
        for (auto& s : set_layouts)
        {
            if (s.set_number == i)
            {
                for (auto& b : s.bindings)
                {
                    auto it = binds.find(b.binding);
                    if (it == binds.end())
                    {
                        binds[b.binding] = b;
                    }
                    else
                    {
                        binds[b.binding].stageFlags |= b.stageFlags;
                    }
                }
            }
        }
        for (auto [k, v] : binds)
        {
            ly.bindings.push_back(v);
        }
        // sort the bindings, for hash purposes
        std::sort(ly.bindings.begin(), ly.bindings.end(),
                  [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b)
                  { return a.binding < b.binding; });

        ly.create_info.bindingCount = (uint32_t)ly.bindings.size();
        ly.create_info.pBindings = ly.bindings.data();
        ly.create_info.flags = 0;
        ly.create_info.pNext = 0;

        if (ly.create_info.bindingCount > 0)
        {
            se.m_set_hashes[i] = hash_descriptor_layout_info(&ly.create_info);
            vkCreateDescriptorSetLayout(engine->vk_device(), &ly.create_info, nullptr,
                                        &se.m_set_layouts[i]);
        }
        else
        {
            se.m_set_hashes[i] = 0;
            se.m_set_layouts[i] = VK_NULL_HANDLE;
        }
    }

    // we start from just the default empty pipeline layout info
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = utils::pipeline_layout_create_info();

    // setup push constants
    VkPushConstantRange push_constant;
    // offset 0
    push_constant.offset = 0;
    // size of a MeshPushConstant struct
    push_constant.size = sizeof(mesh_push_constants);
    // for the vertex shader
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    mesh_pipeline_layout_info.pPushConstantRanges = constant_ranges.data();
    mesh_pipeline_layout_info.pushConstantRangeCount = (uint32_t)constant_ranges.size();

    std::array<VkDescriptorSetLayout, 4> compactedLayouts;
    int s = 0;
    for (int i = 0; i < 4; i++)
    {
        if (se.m_set_layouts[i] != VK_NULL_HANDLE)
        {
            compactedLayouts[s] = se.m_set_layouts[i];
            s++;
        }
    }

    mesh_pipeline_layout_info.setLayoutCount = s;
    mesh_pipeline_layout_info.pSetLayouts = compactedLayouts.data();

    vkCreatePipelineLayout(engine->vk_device(), &mesh_pipeline_layout_info, nullptr,
                           &se.m_build_layout);
}

mesh_data*
loader::load_mesh(const agea::utils::id& mesh_id,
                  const agea::utils::path& external_path,
                  const agea::utils::path& idx_file,
                  const agea::utils::path& vert_file)
{
    auto device = glob::render_device::get();

    auto itr = m_meshes_cache.find(mesh_id);

    if (itr != m_meshes_cache.end())
    {
        return itr->second.get();
    }

    auto md = std::make_shared<mesh_data>();
    md->m_id = mesh_id;

    if (!external_path.empty())
    {
        auto path = glob::resource_locator::get()->resource(category::assets, external_path.str());
        if (!vulkan_mesh_data_loader::load_from_obj(path, *md))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }
    else
    {
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
    stagingBuffer = allocated_buffer::create(
        []() { return glob::render_device::get()->allocator(); }, stagingBufferInfo, vmaallocInfo);

    // copy vertex data
    char* data = nullptr;
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

    md->m_vertexBuffer = allocated_buffer::create(
        []() { return glob::render_device::get()->allocator(); }, vertexBufferInfo, vmaallocInfo);
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
        md->m_indexBuffer =
            allocated_buffer::create([]() { return glob::render_device::get()->allocator(); },
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

bool
loader::create_default_material(VkPipeline pipeline,
                                shader_effect* effect,
                                const agea::utils::id& id)
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
loader::load_texture(const agea::utils::id& texture_id, const std::string& base_color)
{
    auto device = glob::render_device::get();

    auto titr = m_textures_cache.find(texture_id);
    if (titr != m_textures_cache.end())
    {
        return titr->second.get();
    }

    auto td = std::make_shared<texture_data>();
    td->m_device = []() { return glob::render_device::get()->vk_device(); };

    if (base_color.front() == '#')
    {
        if (!load_1x1_image_from_color(base_color, td->image))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }
    else
    {
        if (!load_image_from_file_r(base_color, td->image))
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }

    VkImageViewCreateInfo imageinfo = utils::imageview_create_info(
        VK_FORMAT_R8G8B8A8_UNORM, td->image.image(), VK_IMAGE_ASPECT_COLOR_BIT);
    imageinfo.subresourceRange.levelCount = td->image.get_mip_levels();
    vkCreateImageView(device->vk_device(), &imageinfo, nullptr, &td->image_view);

    m_textures_cache[texture_id] = td;

    return td.get();
}

material_data*
loader::load_material(const agea::utils::id& material_id,
                      const agea::utils::id& texture_id,
                      const agea::utils::id& base_effect_id)
{
    auto device = glob::render_device::get();

    auto it = m_materials_cache.find(material_id);
    if (it != m_materials_cache.end())
    {
        return it->second.get();
    }

    auto td = std::make_shared<material_data>();
    auto& def = m_materials_cache.at(base_effect_id);
    td->id = material_id;

    td->pipeline = def->pipeline;
    td->effect = def->effect;

    m_materials_cache[material_id] = td;

    VkDescriptorImageInfo imageBufferInfo{};
    imageBufferInfo.sampler = device->sampler("default");
    imageBufferInfo.imageView = m_textures_cache[texture_id]->image_view;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        device->descriptor_allocator())
        .bind_image(0, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(td->texture_set);

    return td.get();
}

shader_data*
loader::load_shader(const agea::utils::id& path)
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
    if (!agea::utils::file_utils::load_file(full_path, buffer))
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

    auto sd = std::make_shared<shader_data>(
        []() { return glob::render_device::get()->vk_device(); }, module, std::move(buffer));

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

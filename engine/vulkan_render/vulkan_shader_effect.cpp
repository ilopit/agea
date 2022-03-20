#include "vulkan_render/vulkan_shader_effect.h"

#include "render_device.h"
#include "vulkan_shader_data.h"
#include "vk_initializers.h"
#include "vk_engine.h"

#include "utils/file_utils.h"

#include <spirv_reflect.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>

namespace agea
{
namespace render
{

// FNV-1a 32bit hashing algorithm.
constexpr uint32_t
fnv1a_32(char const* s, std::size_t count)
{
    return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
}
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

void
shader_effect::add_stage(shader_data* shaderModule, VkShaderStageFlagBits stage)
{
    shader_stage newStage = {shaderModule, stage};
    m_stages.push_back(newStage);
}

struct DescriptorSetLayoutData
{
    uint32_t set_number;
    VkDescriptorSetLayoutCreateInfo create_info;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

void
shader_effect::reflect_layout(render_device* engine,
                              reflection_overrides* overrides,
                              int overrideCount)
{
    std::vector<DescriptorSetLayoutData> set_layouts;

    std::vector<VkPushConstantRange> constant_ranges;

    for (auto& s : m_stages)
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

                reflected_binding reflected;
                reflected.m_binding = layout_binding.binding;
                reflected.m_set = refl_set.set;
                reflected.m_type = layout_binding.descriptorType;

                m_bindings[refl_binding.name] = reflected;
            }
            layout.set_number = refl_set.set;
            layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout.create_info.bindingCount = refl_set.binding_count;
            layout.create_info.pBindings = layout.bindings.data();

            set_layouts.push_back(layout);
        }

        // pushconstants

        result = spvReflectEnumeratePushConstants(&spvmodule, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectBlockVariable*> pconstants(count);
        result = spvReflectEnumeratePushConstants(&spvmodule, &count, pconstants.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        if (count > 0)
        {
            VkPushConstantRange pcs{};
            pcs.offset = pconstants[0]->offset;
            pcs.size = pconstants[0]->size;
            pcs.stageFlags = s.m_stage;

            constant_ranges.push_back(pcs);
        }
    }

    std::array<DescriptorSetLayoutData, 4> merged_layouts;

    for (int i = 0; i < 4; i++)
    {
        DescriptorSetLayoutData& ly = merged_layouts[i];

        ly.set_number = i;

        ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        for (auto& s : set_layouts)
        {
            if (s.set_number == i)
            {
                for (auto& b : s.bindings)
                {
                    ly.bindings.push_back(b);
                }
            }
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
            m_set_hashes[i] = hash_descriptor_layout_info(&ly.create_info);
            vkCreateDescriptorSetLayout(engine->vk_device(), &ly.create_info, nullptr,
                                        &m_set_layouts[i]);
        }
        else
        {
            m_set_hashes[i] = 0;
            m_set_layouts[i] = VK_NULL_HANDLE;
        }
    }

    // we start from just the default empty pipeline layout info
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vk_init::pipeline_layout_create_info();

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
        if (m_set_layouts[i] != VK_NULL_HANDLE)
        {
            compactedLayouts[s] = m_set_layouts[i];
            s++;
        }
    }

    mesh_pipeline_layout_info.setLayoutCount = s;
    mesh_pipeline_layout_info.pSetLayouts = compactedLayouts.data();

    vkCreatePipelineLayout(engine->vk_device(), &mesh_pipeline_layout_info, nullptr,
                           &m_build_layout);
}

void
shader_descriptor_binder::bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo)
{
    bind_dynamic_buffer(name, -1, bufferInfo);
}

void
shader_descriptor_binder::bind_dynamic_buffer(const char* name,
                                              uint32_t offset,
                                              const VkDescriptorBufferInfo& bufferInfo)
{
    auto found = m_shaders->m_bindings.find(name);
    if (found != m_shaders->m_bindings.end())
    {
        const shader_effect::reflected_binding& bind = (*found).second;

        for (auto& write : m_buffer_writes)
        {
            if (write.m_dstBinding == bind.m_binding && write.m_dstSet == bind.m_set)
            {
                if (write.m_bufferInfo.buffer != bufferInfo.buffer ||
                    write.m_bufferInfo.range != bufferInfo.range ||
                    write.m_bufferInfo.offset != bufferInfo.offset)
                {
                    write.m_bufferInfo = bufferInfo;
                    write.m_dynamic_offset = offset;

                    m_cached_descriptor_sets[write.m_dstSet] = VK_NULL_HANDLE;
                }
                else
                {
                    // already in the write list, but matches buffer
                    write.m_dynamic_offset = offset;
                }

                return;
            }
        }

        buffer_write_descriptor newWrite;
        newWrite.m_dstSet = bind.m_set;
        newWrite.m_dstBinding = bind.m_binding;
        newWrite.m_descriptor_type = bind.m_type;
        newWrite.m_bufferInfo = bufferInfo;
        newWrite.m_dynamic_offset = offset;

        m_cached_descriptor_sets[bind.m_set] = VK_NULL_HANDLE;

        m_buffer_writes.push_back(newWrite);
    }
}

void
shader_descriptor_binder::apply_binds(VkCommandBuffer cmd)
{
    for (int i = 0; i < 2; i++)
    {
        // there are writes for this set
        if (m_cached_descriptor_sets[i] != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaders->m_build_layout,
                                    i, 1, &m_cached_descriptor_sets[i], m_set_offsets[i].count,
                                    m_set_offsets[i].m_offsets.data());
        }
    }
}

void
shader_descriptor_binder::build_sets(VkDevice device, vk_utils::descriptor_allocator& allocator)
{
    std::array<std::vector<VkWriteDescriptorSet>, 4> writes{};

    std::sort(m_buffer_writes.begin(), m_buffer_writes.end(),
              [](buffer_write_descriptor& a, buffer_write_descriptor& b)
              {
                  if (b.m_dstSet == a.m_dstSet)
                  {
                      return a.m_dstSet < b.m_dstSet;
                  }
                  else
                  {
                      return a.m_dstBinding < b.m_dstBinding;
                  }
              });

    // reset the dynamic offsets
    for (auto& s : m_set_offsets)
    {
        s.count = 0;
    }

    for (buffer_write_descriptor& w : m_buffer_writes)
    {
        uint32_t set = w.m_dstSet;
        VkWriteDescriptorSet write = vk_init::write_descriptor_buffer(
            w.m_descriptor_type, VK_NULL_HANDLE, &w.m_bufferInfo, w.m_dstBinding);

        writes[set].push_back(write);

        // dynamic offsets
        if (w.m_descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            w.m_descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
            dyn_offsets& offsetSet = m_set_offsets[set];
            offsetSet.m_offsets[offsetSet.count] = w.m_dynamic_offset;
            offsetSet.count++;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        // there are writes for this set
        if (writes[i].size() > 0)
        {
            if (m_cached_descriptor_sets[i] == VK_NULL_HANDLE)
            {
                // alloc
                auto layout = m_shaders->m_set_layouts[i];

                VkDescriptorSet newDescriptor;
                allocator.allocate(&newDescriptor, layout);

                for (auto& w : writes[i])
                {
                    w.dstSet = newDescriptor;
                }
                vkUpdateDescriptorSets(device, (uint32_t)writes[i].size(), writes[i].data(), 0,
                                       nullptr);

                m_cached_descriptor_sets[i] = newDescriptor;
            }
        }
    }
}

void
shader_descriptor_binder::set_shader(shader_effect* newShader)
{
    // invalidate nonequal layouts
    if (m_shaders && m_shaders != newShader)
    {
        for (int i = 0; i < 4; i++)
        {
            if (newShader->m_set_hashes[i] != m_shaders->m_set_hashes[i])
            {
                m_cached_descriptor_sets[i] = VK_NULL_HANDLE;
            }
            else if (newShader->m_set_hashes[i] == 0)
            {
                m_cached_descriptor_sets[i] = VK_NULL_HANDLE;
            }
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            m_cached_descriptor_sets[i] = VK_NULL_HANDLE;
        }
    }

    m_shaders = newShader;
}
}  // namespace render
}  // namespace agea

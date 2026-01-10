#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/shader_reflection_utils.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/utils/vulkan_initializers.h"

#include <shader_system/shader_compiler.h>

#include <utils/string_utility.h>
#include <utils/file_utils.h>
#include <utils/buffer.h>
#include <utils/process.h>
#include <utils/kryga_log.h>

#include <global_state/global_state.h>
#include <resource_locator/resource_locator.h>

namespace kryga
{
namespace render
{

namespace
{

result_code
load_compute_shader_module(const kryga::utils::buffer& input,
                           std::shared_ptr<shader_module_data>& sd)
{
    auto device = glob::render_device::get();

    auto result = shader_compiler::compile_shader(input);
    if (!result)
    {
        return result.error();
    }
    auto buffer = std::move(result.value().raw_data);

    VkShaderModuleCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                           .pNext = VK_NULL_HANDLE,
                                           .codeSize = buffer.size(),
                                           .pCode = (uint32_t*)buffer.data()};

    VkShaderModule module;
    if (vkCreateShaderModule(device->vk_device(), &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    sd = std::make_shared<shader_module_data>(module, std::move(buffer),
                                              VK_SHADER_STAGE_COMPUTE_BIT);

    return result_code::ok;
}  // namespace

}  // namespace

bool
vulkan_compute_shader_loader::create_compute_pipeline_layout(compute_shader_data& cs)
{
    auto device = glob::render_device::get();

    // Get descriptor set layouts from reflection
    const auto& reflection = cs.m_compute_stage_reflection;

    std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTORS_SETS_COUNT> layout_cis;
    std::array<std::vector<VkDescriptorSetLayoutBinding>, DESCRIPTORS_SETS_COUNT> bindings_storage;

    for (uint32_t i = 0; i < DESCRIPTORS_SETS_COUNT; ++i)
    {
        layout_cis[i] = {};
        layout_cis[i].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    }

    // Populate bindings from reflection
    // Note: shader_reflection uses 'descriptors' and each descriptor_set has 'location' for set
    // index and 'bindigns' (typo in original code) for bindings
    for (const auto& ds : reflection.descriptors)
    {
        if (ds.location < DESCRIPTORS_SETS_COUNT)
        {
            for (const auto& binding : ds.bindigns)  // Note: typo in original struct
            {
                VkDescriptorSetLayoutBinding vk_binding = {};
                vk_binding.binding = binding.location;
                vk_binding.descriptorType = binding.type;
                vk_binding.descriptorCount =
                    binding.descriptors_count > 0 ? binding.descriptors_count : 1;
                vk_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                vk_binding.pImmutableSamplers = nullptr;

                bindings_storage[ds.location].push_back(vk_binding);
            }
        }
    }

    // Create descriptor set layouts
    for (uint32_t i = 0; i < DESCRIPTORS_SETS_COUNT; ++i)
    {
        layout_cis[i].bindingCount = (uint32_t)bindings_storage[i].size();
        layout_cis[i].pBindings = bindings_storage[i].data();

        vkCreateDescriptorSetLayout(device->vk_device(), &layout_cis[i], nullptr,
                                    &cs.m_set_layout[i]);
    }

    // Create pipeline layout
    std::vector<VkPushConstantRange> push_constant_ranges;
    for (const auto& pc : reflection.constants)
    {
        VkPushConstantRange range = {};
        range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset = pc.offset;
        range.size = pc.size;
        push_constant_ranges.push_back(range);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_ci = vk_utils::make_pipeline_layout_create_info();
    pipeline_layout_ci.setLayoutCount = DESCRIPTORS_SETS_COUNT;
    pipeline_layout_ci.pSetLayouts = cs.m_set_layout.data();
    pipeline_layout_ci.pushConstantRangeCount = (uint32_t)push_constant_ranges.size();
    pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();

    vkCreatePipelineLayout(device->vk_device(), &pipeline_layout_ci, nullptr,
                           &cs.m_pipeline_layout);

    return cs.m_pipeline_layout != VK_NULL_HANDLE;
}

result_code
vulkan_compute_shader_loader::create_compute_shader(compute_shader_data& cs_data,
                                                    const kryga::utils::buffer& shader_buffer)
{
    auto device = glob::render_device::get();

    // Load and compile compute shader
    auto rc = load_compute_shader_module(shader_buffer, cs_data.m_compute_stage);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    // Build reflection data
    if (!shader_reflection_utils::build_shader_reflection(
            device, cs_data.m_compute_stage_reflection, cs_data.m_compute_stage))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    // Create pipeline layout
    if (!create_compute_pipeline_layout(cs_data))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    // Create compute pipeline
    VkPipelineShaderStageCreateInfo shader_stage_ci = {};
    shader_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_ci.module = cs_data.m_compute_stage->vk_module();
    shader_stage_ci.pName = "main";

    VkComputePipelineCreateInfo pipeline_ci = {};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_ci.stage = shader_stage_ci;
    pipeline_ci.layout = cs_data.m_pipeline_layout;

    if (vkCreateComputePipelines(device->vk_device(), VK_NULL_HANDLE, 1, &pipeline_ci, nullptr,
                                 &cs_data.m_pipeline) != VK_SUCCESS)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return result_code::ok;
}

}  // namespace render
}  // namespace kryga

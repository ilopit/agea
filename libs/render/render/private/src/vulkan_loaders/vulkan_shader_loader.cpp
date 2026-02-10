#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"

#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/kryga_render.h"
#include "gpu_types/gpu_generic_constants.h"
#include "vulkan_render/vk_pipeline_builder.h"
#include "vulkan_render/shader_reflection_utils.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "shader_system/shader_compiler.h"

#include <utils/string_utility.h>
#include <utils/file_utils.h>
#include <utils/buffer.h>
#include <utils/process.h>
#include <utils/kryga_log.h>

#include <global_state/global_state.h>

#include <native/native_window.h>

#include <resource_locator/resource_locator.h>
#include <serialization/serialization.h>

#include <vk_mem_alloc.h>
#include <stb_unofficial/stb.h>

namespace kryga
{

namespace render
{

kryga::result_code
load_data_shader(const kryga::utils::buffer& input,
                 bool is_binary,
                 VkShaderStageFlagBits stage_bit,
                 std::shared_ptr<shader_module_data>& sd)
{
    auto device = glob::glob_state().get_render_device();

    compiled_shader compiled;

    if (!is_binary)
    {
        auto rc = shader_compiler::compile_shader(input);
        if (!rc)
        {
            return rc.error();
        }
        compiled = std::move(rc.value());
    }
    else
    {
        KRG_never("Not supported");
    }

    VkShaderModuleCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                           .pNext = VK_NULL_HANDLE,
                                           .codeSize = compiled.spirv.size(),
                                           .pCode = (uint32_t*)compiled.spirv.data()};

    VkShaderModule module = VK_NULL_HANDLE;

    if (vkCreateShaderModule(device->vk_device(), &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    sd = std::make_shared<shader_module_data>(module, std::move(compiled.spirv), stage_bit,
                                              std::move(compiled.reflection));

    return result_code::ok;
}

bool
vulkan_shader_loader::create_shader_effect_pipeline_layout(shader_effect_data& se)
{
    auto device = glob::glob_state().get_render_device();
    std::vector<vulkan_descriptor_set_layout_data> set_layouts;
    se.generate_set_layouts(set_layouts);

    std::vector<VkPushConstantRange> constants;
    se.generate_constants(constants);

    std::array<vulkan_descriptor_set_layout_data, DESCRIPTORS_SETS_COUNT> merged_layouts;

    for (uint32_t i = 0; i < DESCRIPTORS_SETS_COUNT; i++)
    {
        vulkan_descriptor_set_layout_data& ly = merged_layouts[i];
        ly.set_idx = i;
        ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> binds;
        for (auto& s : set_layouts)
        {
            if (s.set_idx == i)
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

        for (auto& [k, v] : binds)
        {
            ly.bindings.push_back(v);
        }

        std::sort(ly.bindings.begin(), ly.bindings.end(),
                  [](VkDescriptorSetLayoutBinding a, VkDescriptorSetLayoutBinding b)
                  { return a.binding < b.binding; });

        ly.create_info.bindingCount = (uint32_t)ly.bindings.size();
        ly.create_info.pBindings = ly.bindings.data();
        ly.create_info.flags = 0;
        ly.create_info.pNext = 0;

        // For set 2 (textures), skip creating layout - we'll use the global bindless layout
        // Don't store it in se.m_set_layout since it's shared and shouldn't be destroyed
        if (i == KGPU_textures_descriptor_sets)
        {
            se.m_set_layout[i] = VK_NULL_HANDLE;  // Mark as not owned
            continue;
        }

        vkCreateDescriptorSetLayout(device->vk_device(), &ly.create_info, nullptr,
                                    &se.m_set_layout[i]);
    }

    // Build pipeline layout using the global bindless layout for set 2
    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> pipeline_layouts = se.m_set_layout;
    auto bindless_layout = glob::glob_state().get_vulkan_render()->get_bindless_layout();
    if (bindless_layout != VK_NULL_HANDLE)
    {
        pipeline_layouts[KGPU_textures_descriptor_sets] = bindless_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_ci = vk_utils::make_pipeline_layout_create_info();

    pipeline_layout_ci.pPushConstantRanges = constants.data();
    pipeline_layout_ci.pushConstantRangeCount = (uint32_t)constants.size();

    pipeline_layout_ci.setLayoutCount = DESCRIPTORS_SETS_COUNT;
    pipeline_layout_ci.pSetLayouts = pipeline_layouts.data();

    vkCreatePipelineLayout(device->vk_device(), &pipeline_layout_ci, nullptr,
                           &se.m_pipeline_layout);

    return se.m_pipeline_layout != VK_NULL_HANDLE;
}

result_code
vulkan_shader_loader::create_shader_effect(shader_effect_data& se_data,
                                           std::shared_ptr<shader_module_data>& vert_module,
                                           std::shared_ptr<shader_module_data>& frag_module,
                                           const shader_effect_create_info& info)
{
    se_data.m_is_wire = info.is_wire;
    se_data.m_enable_alpha = info.alpha != alpha_mode::none;

    se_data.m_vertex_stage = vert_module;
    se_data.m_frag_stage = frag_module;

    if (info.expected_input_vertex_layout)
    {
        se_data.m_expected_vertex_input = info.expected_input_vertex_layout;
    }

    if (!shader_reflection_utils::are_layouts_compatible(
            se_data.m_expected_vertex_input,
            se_data.m_vertex_stage->get_reflection().input_interface.layout, false, false))
    {
        // Adopt reflected layout — shader defines the authoritative vertex input format
        se_data.m_expected_vertex_input =
            se_data.m_vertex_stage->get_reflection().input_interface.layout;
    }

    if (!shader_reflection_utils::are_layouts_compatible(
            se_data.m_vertex_stage->get_reflection().output_interface.layout,
            se_data.m_frag_stage->get_reflection().input_interface.layout, true, false))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    // Validate fragment shader outputs are compatible with render pass attachments
    if (info.rp && info.rp->get_color_attachment_count() > 0)
    {
        if (!info.rp->validate_fragment_outputs(
                se_data.m_frag_stage->get_reflection().output_interface))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
    }

    if (!create_shader_effect_pipeline_layout(se_data))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    vk_utils::pipeline_builder pb;
    pb.m_vertex_input_info_ci = vk_utils::make_vertex_input_state_create_info();

    pb.m_input_assembly_ci =
        vk_utils::make_input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    auto width = info.width ? info.width : (uint32_t)glob::glob_state().get_native_window()->get_size().w;
    auto height = info.height ? info.height : (uint32_t)glob::glob_state().get_native_window()->get_size().h;

    pb.m_viewport.x = 0.0f;
    pb.m_viewport.y = 0.0f;
    pb.m_viewport.width = (float)width;
    pb.m_viewport.height = (float)height;

    pb.m_viewport.minDepth = 0.0f;
    pb.m_viewport.maxDepth = 1.0f;

    pb.m_scissor.offset = {0, 0};
    pb.m_scissor.extent = VkExtent2D{width, height};

    pb.m_rasterizer_ci = vk_utils::make_rasterization_state_create_info(
        info.is_wire ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL, info.cull_mode);

    pb.m_multisampling_ci = vk_utils::make_multisampling_state_create_info();

    pb.m_color_blend_attachment = vk_utils::make_color_blend_attachment_state(info.alpha);

    pb.m_depth_stencil_ci =
        vk_utils::make_depth_stencil_create_info(true, true, info.depth_compare_op, info.ds_mode);

    auto vert_input_description =
        render::convert_to_vertex_input_description(*se_data.m_expected_vertex_input);

    pb.m_vertex_input_info_ci.pVertexAttributeDescriptions =
        vert_input_description.attributes.data();
    pb.m_vertex_input_info_ci.vertexAttributeDescriptionCount =
        (uint32_t)vert_input_description.attributes.size();

    pb.m_vertex_input_info_ci.pVertexBindingDescriptions = vert_input_description.bindings.data();
    pb.m_vertex_input_info_ci.vertexBindingDescriptionCount =
        (uint32_t)vert_input_description.bindings.size();

    if (info.enable_dynamic_state)
    {
        std::vector<VkDynamicState> dynamic_state_enables = {VK_DYNAMIC_STATE_VIEWPORT,
                                                             VK_DYNAMIC_STATE_SCISSOR};

        pb.m_dynamic_state_enables = dynamic_state_enables;
    }

    auto vert_shader = vert_module->vk_module();
    pb.m_shader_stages_ci.push_back(
        vk_utils::make_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vert_shader));

    auto frag_shader = frag_module->vk_module();
    pb.m_shader_stages_ci.push_back(vk_utils::make_pipeline_shader_stage_create_info(
        VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader));

    pb.m_pipeline_layout = se_data.m_pipeline_layout;
    auto& device = glob::glob_state().getr_render_device();
    se_data.m_pipeline = pb.build(device.vk_device(), info.rp->vk());

    pb.m_depth_stencil_ci = vk_utils::make_depth_stencil_create_info(
        true, true, info.depth_compare_op, depth_stencil_mode::stencil);

    se_data.m_with_stencil_pipeline = pb.build(device.vk_device(), info.rp->vk());

    return se_data.m_pipeline != VK_NULL_HANDLE ? result_code::ok : result_code::failed;
}

result_code
vulkan_shader_loader::update_shader_effect(shader_effect_data& se_data,
                                           const shader_effect_create_info& info,
                                           std::shared_ptr<render::shader_effect_data>& old_se_data)
{
    auto device = glob::glob_state().get_render_device();

    old_se_data = std::make_shared<render::shader_effect_data>(se_data.get_id());

    if (se_data.m_vertex_stage)
    {
        old_se_data->m_vertex_stage = std::move(se_data.m_vertex_stage);
    }

    auto rc = load_data_shader(*info.vert_buffer, info.is_vert_binary, VK_SHADER_STAGE_VERTEX_BIT,
                               se_data.m_vertex_stage);
    if (rc != result_code::ok)
    {
        return rc;
    }

    if (se_data.m_frag_stage)
    {
        old_se_data->m_frag_stage = std::move(se_data.m_frag_stage);
    }

    rc = load_data_shader(*info.frag_buffer, info.is_frag_binary, VK_SHADER_STAGE_FRAGMENT_BIT,
                          se_data.m_frag_stage);
    if (rc != result_code::ok)
    {
        return rc;
    }

    old_se_data->m_set_layout = std::move(se_data.m_set_layout);

    old_se_data->m_pipeline = se_data.m_pipeline;
    old_se_data->m_with_stencil_pipeline = se_data.m_with_stencil_pipeline;
    se_data.m_pipeline = VK_NULL_HANDLE;
    se_data.m_with_stencil_pipeline = VK_NULL_HANDLE;

    old_se_data->m_pipeline_layout = se_data.m_pipeline_layout;
    se_data.m_pipeline_layout = VK_NULL_HANDLE;

    return create_shader_effect(se_data, se_data.m_vertex_stage, se_data.m_frag_stage, info);
}

result_code
vulkan_shader_loader::create_shader_effect(shader_effect_data& se_data,
                                           const shader_effect_create_info& info)
{
    std::shared_ptr<shader_module_data> vert_module;
    auto rc = load_data_shader(*info.vert_buffer, info.is_vert_binary, VK_SHADER_STAGE_VERTEX_BIT,
                               vert_module);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    std::shared_ptr<shader_module_data> frag_module;
    rc = load_data_shader(*info.frag_buffer, info.is_vert_binary, VK_SHADER_STAGE_FRAGMENT_BIT,
                          frag_module);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    return create_shader_effect(se_data, vert_module, frag_module, info);
}

}  // namespace render
}  // namespace kryga

#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"

#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/vulkan_render_device.h"
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

namespace render
{

namespace
{

result_code
compile_shader(const agea::utils::buffer& raw_buffer, agea::utils::buffer& compiled_buffer)
{
    static int shader_id = 0;

    ipc::construct_params params;
    params.path_to_binary =
        (glob::resource_locator::get()->resource_dir(category::tools) / "glslc.exe");

    static auto td = glob::resource_locator::get()->temp_dir();
    params.working_dir = *td.folder;

    auto shader_name = APATH(std::to_string(shader_id));

    agea::utils::path compiled_path = *td.folder / shader_name.str();
    compiled_path.add(".spv");

    auto includes = glob::resource_locator::get()->resource_dir(category::shaders_includes);
    params.arguments =
        std::format("-V {0} -o {1} --target-env=vulkan1.2 --target-spv=spv1.5 -I {2}",
                    raw_buffer.get_file().str(), compiled_path.str(), includes.str());

    uint64_t rc = 0;
    if (!ipc::run_binary(params, rc) || rc != 0)
    {
        return result_code::compilation_failed;
    }

    if (!agea::utils::buffer::load(compiled_path, compiled_buffer))
    {
        ALOG_FATAL("Shader compilation failed");
        return result_code::failed;
    }

    return result_code::ok;
}

agea::result_code
load_data_shader(const agea::utils::buffer& input,
                 bool is_binary,
                 VkShaderStageFlagBits stage_bit,
                 std::shared_ptr<shader_module_data>& sd)
{
    auto device = glob::render_device::get();

    agea::utils::buffer compiled_buffer;

    if (!is_binary)
    {
        auto rc = compile_shader(input, compiled_buffer);
        if (rc != result_code::ok)
        {
            return rc;
        }
    }
    else
    {
        AGEA_never("Not supported");
        compiled_buffer = input;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.codeSize = compiled_buffer.size();
    createInfo.pCode = (uint32_t*)compiled_buffer.data();

    VkShaderModule module;

    if (vkCreateShaderModule(device->vk_device(), &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    sd = std::make_shared<shader_module_data>(module, std::move(compiled_buffer), stage_bit);

    return result_code::ok;
}

}  // namespace

bool
vulkan_shader_loader::create_shader_effect_pipeline_layout(shader_effect_data& se)
{
    auto device = glob::render_device::get();
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

        vkCreateDescriptorSetLayout(device->vk_device(), &ly.create_info, nullptr,
                                    &se.m_set_layout[i]);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_ci = vk_utils::make_pipeline_layout_create_info();

    pipeline_layout_ci.pPushConstantRanges = constants.data();
    pipeline_layout_ci.pushConstantRangeCount = (uint32_t)constants.size();

    pipeline_layout_ci.setLayoutCount = DESCRIPTORS_SETS_COUNT;
    pipeline_layout_ci.pSetLayouts = se.m_set_layout.data();

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

    auto device = glob::render_device::get();

    if (!shader_reflection_utils::build_shader_reflection(
            device, se_data.m_vertext_stage_reflection, se_data.m_vertex_stage))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    if (!shader_reflection_utils::are_layouts_compatible(
            se_data.m_expected_vertex_input,
            se_data.m_vertext_stage_reflection.input_interface.layout, false, false))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    if (!shader_reflection_utils::build_shader_reflection(
            device, se_data.m_fragment_stage_reflection, se_data.m_frag_stage))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    if (!shader_reflection_utils::are_layouts_compatible(
            se_data.m_vertext_stage_reflection.output_interface.layout,
            se_data.m_fragment_stage_reflection.input_interface.layout, true, false))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
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

    auto width = info.width ? info.width : (uint32_t)glob::native_window::get()->get_size().w;
    auto height = info.height ? info.height : (uint32_t)glob::native_window::get()->get_size().h;

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

    se_data.m_pipeline = pb.build(device->vk_device(), info.rp->vk());

    pb.m_depth_stencil_ci = vk_utils::make_depth_stencil_create_info(
        true, true, info.depth_compare_op, depth_stencil_mode::stencil);

    se_data.m_with_stencil_pipeline = pb.build(device->vk_device(), info.rp->vk());

    return se_data.m_pipeline != VK_NULL_HANDLE ? result_code::ok : result_code::failed;
}

result_code
vulkan_shader_loader::update_shader_effect(shader_effect_data& se_data,
                                           const shader_effect_create_info& info,
                                           std::shared_ptr<render::shader_effect_data>& old_se_data)
{
    auto device = glob::render_device::get();

    old_se_data = std::make_shared<render::shader_effect_data>(se_data.get_id());

    if (se_data.m_vertex_stage)
    {
        old_se_data->m_vertex_stage = std::move(se_data.m_vertex_stage);
        se_data.m_vertext_stage_reflection = {};
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
        se_data.m_fragment_stage_reflection = {};
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
    auto device = glob::render_device::get();

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
}  // namespace agea

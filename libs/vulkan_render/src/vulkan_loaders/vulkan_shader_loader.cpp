#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"

#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/render_device.h"
#include "vulkan_render/vk_pipeline_builder.h"
#include "vulkan_render/shader_reflection.h"
#include "vulkan_render/vk_transit.h"

#include <vulkan_render_types/vulkan_mesh_data.h>
#include <vulkan_render_types/vulkan_texture_data.h>
#include <vulkan_render_types/vulkan_material_data.h>
#include <vulkan_render_types/vulkan_gpu_types.h>
#include <vulkan_render_types/vulkan_initializers.h>
#include <vulkan_render_types/vulkan_shader_data.h>
#include <vulkan_render_types/vulkan_shader_effect_data.h>

#include <utils/string_utility.h>
#include <utils/file_utils.h>
#include <utils/buffer.h>
#include <utils/process.h>

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

void
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

    params.arguments = "-V " + raw_buffer.get_file().str() + " -o " + compiled_path.str() +
                       " --target-env=vulkan1.2 --target-spv=spv1.5";

    uint64_t rc = 0;
    if (!ipc::run_binary(params, rc))
    {
        AGEA_never("Shader compilation failed");
        return;
    }

    if (!agea::utils::buffer::load(compiled_path, compiled_buffer))
    {
        ALOG_FATAL("Shader compilation failed");
    }
}

std::shared_ptr<shader_data>
load_data_shader(const agea::utils::buffer& input, bool is_binary, VkShaderStageFlagBits stage_bit)
{
    auto device = glob::render_device::get();

    agea::utils::buffer compiled_buffer;

    if (!is_binary)
    {
        compile_shader(input, compiled_buffer);
    }
    else
    {
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
        return nullptr;
    }

    auto sd = std::make_shared<shader_data>(device->get_vk_device_provider(), module,
                                            std::move(compiled_buffer), stage_bit);

    return sd;
}

}  // namespace

bool
vulkan_shader_loader::create_shader_effect_pipeline(shader_effect_data& se)
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
        ly.set_number = i;
        ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> binds;
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

        if (ly.create_info.bindingCount > 0)
        {
            vkCreateDescriptorSetLayout(device->vk_device(), &ly.create_info, nullptr,
                                        &se.m_set_layout[i]);
        }
        else
        {
            se.m_set_layout[i] = VK_NULL_HANDLE;
        }
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = utils::pipeline_layout_create_info();

    VkPushConstantRange push_constants{};
    push_constants.offset = 0;
    push_constants.size = sizeof(gpu_push_constants);
    push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    pipeline_layout_info.pPushConstantRanges = constants.data();
    pipeline_layout_info.pushConstantRangeCount = (uint32_t)constants.size();

    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> compactedLayouts{};

    uint32_t s = 0;

    for (uint32_t i = 0; i < DESCRIPTORS_SETS_COUNT; i++)
    {
        if (se.m_set_layout[i] != VK_NULL_HANDLE)
        {
            compactedLayouts[s] = se.m_set_layout[i];
            ++s;
        }
    }

    pipeline_layout_info.setLayoutCount = s;
    pipeline_layout_info.pSetLayouts = compactedLayouts.data();

    vkCreatePipelineLayout(device->vk_device(), &pipeline_layout_info, nullptr,
                           &se.m_pipeline_layout);

    return true;
}

bool
vulkan_shader_loader::create_shader_effect(shader_effect_data& se_data,
                                           std::shared_ptr<shader_data>& vert_module,
                                           std::shared_ptr<shader_data>& frag_module,
                                           bool is_wire,
                                           bool enable_alpha,
                                           VkRenderPass render_pass)
{
    se_data.m_is_wire = is_wire;
    se_data.m_enable_alpha = enable_alpha;

    pipeline_builder pb;

    se_data.add_shader(vert_module);
    se_data.add_shader(frag_module);

    auto device = glob::render_device::get();

    if (!build_shader_reflection(device, se_data))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!create_shader_effect_pipeline(se_data))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    pb.m_vertex_input_info = utils::vertex_input_state_create_info();

    pb.m_input_assembly = utils::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    pb.m_viewport.x = 0.0f;
    pb.m_viewport.y = 0.0f;
    pb.m_viewport.width = (float)width;
    pb.m_viewport.height = (float)height;
    pb.m_viewport.minDepth = 0.0f;
    pb.m_viewport.maxDepth = 1.0f;

    pb.m_scissor.offset = {0, 0};
    pb.m_scissor.extent = VkExtent2D{width, height};

    pb.m_rasterizer = utils::rasterization_state_create_info(!is_wire, enable_alpha);

    pb.m_multisampling = utils::multisampling_state_create_info();

    pb.m_color_blend_attachment = utils::color_blend_attachment_state(enable_alpha);

    pb.m_depth_stencil = utils::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    auto vertexDescription = render::get_vertex_description();

    pb.m_vertex_input_info.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pb.m_vertex_input_info.vertexAttributeDescriptionCount =
        (uint32_t)vertexDescription.attributes.size();

    pb.m_vertex_input_info.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pb.m_vertex_input_info.vertexBindingDescriptionCount =
        (uint32_t)vertexDescription.bindings.size();

    auto vert_shader = vert_module->vk_module();
    pb.m_shader_stages.push_back(
        utils::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vert_shader));

    auto frag_shader = frag_module->vk_module();
    pb.m_shader_stages.push_back(
        utils::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader));

    pb.m_pipelineLayout = se_data.m_pipeline_layout;

    se_data.m_pipeline = pb.build_pipeline(device->vk_device(), render_pass);

    return true;
}

bool
vulkan_shader_loader::update_shader_effect(shader_effect_data& se_data,
                                           agea::utils::buffer& vert_buffer,
                                           bool is_vert_binary,
                                           agea::utils::buffer& frag_buffer,
                                           bool is_frag_binary,
                                           bool is_wire,
                                           bool enable_alpha,
                                           VkRenderPass render_pass,
                                           std::shared_ptr<render::shader_effect_data>& old_se_data)
{
    auto device = glob::render_device::get();

    old_se_data = std::make_shared<render::shader_effect_data>(se_data.id(),
                                                               device->get_vk_device_provider());

    auto vs = se_data.extract_shader(VK_SHADER_STAGE_VERTEX_BIT);
    if (vert_buffer.consume_file_updated())
    {
        old_se_data->add_shader(std::move(vs));

        vs = load_data_shader(vert_buffer, is_vert_binary, VK_SHADER_STAGE_VERTEX_BIT);
        if (!vs)
        {
            ALOG_ERROR("Error");
            return false;
        }
    }

    auto fs = se_data.extract_shader(VK_SHADER_STAGE_FRAGMENT_BIT);
    if (frag_buffer.consume_file_updated())
    {
        old_se_data->add_shader(std::move(vs));

        vs = load_data_shader(frag_buffer, is_frag_binary, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (!fs)
        {
            ALOG_ERROR("Error");
            return false;
        }
    }

    old_se_data->m_set_layout = std::move(se_data.m_set_layout);

    old_se_data->m_reflection = std::move(se_data.m_reflection);

    old_se_data->m_pipeline = se_data.m_pipeline;
    se_data.m_pipeline = VK_NULL_HANDLE;

    old_se_data->m_pipeline_layout = se_data.m_pipeline_layout;
    se_data.m_pipeline_layout = VK_NULL_HANDLE;

    return create_shader_effect(se_data, vs, fs, is_wire, enable_alpha, render_pass);
}

bool
vulkan_shader_loader::create_shader_effect(shader_effect_data& se_data,
                                           agea::utils::buffer& vert_buffer,
                                           bool is_vert_buffer,
                                           agea::utils::buffer& frag_buffer,
                                           bool is_frag_buffer,
                                           bool is_wire,
                                           bool enable_alpha,
                                           VkRenderPass render_pass)
{
    auto device = glob::render_device::get();

    auto vert_module = load_data_shader(vert_buffer, is_vert_buffer, VK_SHADER_STAGE_VERTEX_BIT);
    if (!vert_module)
    {
        ALOG_ERROR("Error");
        return false;
    }

    auto frag_module = load_data_shader(frag_buffer, is_frag_buffer, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (!frag_module)
    {
        ALOG_ERROR("Error");
        return false;
    }

    return create_shader_effect(se_data, vert_module, frag_module, is_wire, enable_alpha,
                                render_pass);
}

}  // namespace render
}  // namespace agea

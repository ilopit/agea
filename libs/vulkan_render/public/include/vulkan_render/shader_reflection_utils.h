#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan/vulkan.h"

#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <spirv_reflect.h>

namespace agea
{
namespace render
{
class render_device;
struct vulkan_descriptor_set_layout_data;

namespace reflection
{
struct push_constants
{
    std::string name;

    uint32_t offset = 0;
    uint32_t size = 0;

    ::agea::utils::dynobj_layout_sptr layout;
};

struct binding
{
    utils::id name;
    uint32_t location = 0U;
    uint32_t descriptors_count = 0U;
    VkDescriptorType type = VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;

    utils::dynobj_layout_sptr layout;
};

struct descriptor_set
{
    uint32_t location = 0U;
    std::vector<binding> bindigns;
};

struct interface_variables
{
    ::agea::utils::dynobj_layout_sptr layout;
};

struct shader_reflection
{
    std::vector<push_constants> constants;
    std::vector<descriptor_set> descriptors;
    interface_variables input_interface;
    interface_variables output_interface;
};

}  // namespace reflection

struct shader_reflection_utils
{
    static bool
    convert_spvr_to_dyn_layout(const utils::id& field_name,
                               SpvReflectTypeDescription& obj,
                               gpu_dynobj_builder& dl);

    static bool
    are_layouts_compatible(const utils::dynobj_layout_sptr& l,
                           const utils::dynobj_layout_sptr& r,
                           bool non_strict_name_check,
                           bool ignore_highlevel_naming);

    static bool
    compare_dynfield(const utils::dynobj_field& l,
                     const utils::dynobj_field& r,
                     bool non_strict_name_check,
                     bool ignore_highlevel_naming);

    static bool
    are_types_compatible(uint32_t lraw, uint32_t rraw);

    static bool
    extract_interface_variable(std::vector<SpvReflectInterfaceVariable*>& inputs,
                               utils::dynobj_layout_sptr& r);

    static bool
    build_shader_input_reflection(SpvReflectShaderModule& spv_reflection,
                                  reflection::shader_reflection& sr);
    static bool
    build_shader_output_reflection(SpvReflectShaderModule& spv_reflection,
                                   reflection::shader_reflection& sr);

    static bool
    build_shader_descriptor_sets_reflection(SpvReflectShaderModule& spv_reflection,
                                            reflection::shader_reflection& sr);

    static bool
    build_shader_push_constants(SpvReflectShaderModule& spv_reflection,
                                reflection::shader_reflection& sr);

    static bool
    build_shader_reflection(render_device* device,
                            reflection::shader_reflection& sr,
                            std::shared_ptr<shader_module_data>& sd);

    // Converters
    static VkDescriptorSetLayoutBinding
    convert_to_vk_binding(const reflection::binding& b, VkShaderStageFlags stage);

    static void
    convert_to_ds_layout_data(const reflection::descriptor_set& ref_set,
                              VkShaderStageFlags stage,
                              vulkan_descriptor_set_layout_data& layout);

    static void
    convert_to_vk_push_constants(const reflection::push_constants& pc,
                                 VkShaderStageFlags stage,
                                 VkPushConstantRange& range);
};
}  // namespace render
}  // namespace agea
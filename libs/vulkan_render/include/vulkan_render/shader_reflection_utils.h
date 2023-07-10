#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

#include <utils/dynamic_object.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

#include <spirv_reflect.h>

namespace agea
{
namespace render
{
class render_device;

struct shader_reflection_utils
{
    static bool
    convert_spvr_to_dyn_layout(const utils::id& field_name,
                               SpvReflectTypeDescription& obj,
                               gpu_dynobj_builder& dl);

    static bool
    are_layouts_compatible(const utils::dynobj_layout_sptr& l,
                           const utils::dynobj_layout_sptr& r,
                           bool non_strict_name_check);

    static bool
    compare_dynfield(const utils::dynobj_field& l,
                     const utils::dynobj_field& r,
                     bool non_strict_name_check);

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
                            std::shared_ptr<shader_data>& sd);

    static VkDescriptorSetLayoutBinding
    convert_dynobj_to_vk_binding(const utils::dynobj_view<gpu_type>& bind);

    static void
    convert_dynobj_to_layout_data(const utils::dynobj_view<gpu_type>& set_obj,
                                  vulkan_descriptor_set_layout_data& layout);

    static void
    convert_dynobj_to_vk_push_constants(const utils::dynobj_view<gpu_type>& set_obj,
                                        VkPushConstantRange& range);
};
}  // namespace render
}  // namespace agea
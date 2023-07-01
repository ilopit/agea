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
    static void
    spvr_get_fields(SpvReflectTypeDescription& parent, gpu_dynobj_builder& dl);

    static bool
    spvr_to_dyn_layout(SpvReflectTypeDescription* obj, gpu_dynobj_builder& dl);

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
                                  reflection::shader_reflection& sr,
                                  std::shared_ptr<shader_data>& sd);
    static bool
    build_shader_output_reflection(SpvReflectShaderModule& spv_reflection,
                                   reflection::shader_reflection& sr,
                                   std::shared_ptr<shader_data>& sd);

    static bool
    build_shader_reflection(render_device* device,
                            reflection::shader_reflection& sr,
                            std::shared_ptr<shader_data>& sd);
};
}  // namespace render
}  // namespace agea
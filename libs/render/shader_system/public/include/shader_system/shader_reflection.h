#pragma once

#include <gpu_types/dynamic_layout/gpu_types.h>

#include <vulkan/vulkan.h>

#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <spirv_reflect.h>
#include <optional>

namespace kryga::render
{
namespace reflection
{

struct interface_variable
{
    utils::id name;
    uint32_t location = 0;
    gpu_type::id type = gpu_type::nan;
};

struct interface_block
{
    std::vector<interface_variable> variables;
    utils::dynobj_layout_sptr layout;
};

struct push_constants
{
    utils::id name;

    uint32_t offset = 0;
    uint32_t size = 0;

    utils::dynobj_layout_sptr layout;
};

struct binding
{
    utils::id name;
    uint32_t binding_index = 0;
    uint32_t descriptors_count = 1;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;

    utils::dynobj_layout_sptr layout;
};

struct descriptor_set
{
    uint32_t set_index = 0;
    std::vector<binding> bindings;
};

struct compute_info
{
    uint32_t local_size_x = 1;
    uint32_t local_size_y = 1;
    uint32_t local_size_z = 1;
};

struct shader_reflection
{
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    utils::id entry_point;

    std::optional<push_constants> constants;
    std::vector<descriptor_set> descriptors;

    interface_block input_interface;
    interface_block output_interface;

    std::optional<compute_info> compute;

    // Helpers
    bool
    has_push_constants() const
    {
        return constants.has_value();
    }

    const descriptor_set*
    find_set(uint32_t set_idx) const
    {
        for (const auto& ds : descriptors)
        {
            if (ds.set_index == set_idx)
            {
                return &ds;
            }
        }
        return nullptr;
    }

    const binding*
    find_binding(uint32_t set_idx, uint32_t bind_idx) const
    {
        const auto* ds = find_set(set_idx);
        if (!ds)
        {
            return nullptr;
        }
        for (const auto& b : ds->bindings)
        {
            if (b.binding_index == bind_idx)
            {
                return &b;
            }
        }
        return nullptr;
    }
};

}  // namespace reflection

struct shader_reflection_utils
{
    static bool
    convert_spvr_to_dyn_layout(const utils::id& field_name,
                               SpvReflectBlockVariable& block,
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
    extract_interface_variables(std::vector<SpvReflectInterfaceVariable*>& inputs,
                                reflection::interface_block& block);

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
    build_shader_compute_info(SpvReflectShaderModule& spv_reflection,
                              reflection::shader_reflection& sr);

    static bool
    build_shader_reflection(const uint8_t* spirv_code,
                            size_t spirv_size,
                            reflection::shader_reflection& sr);

    // Vulkan converters
    static VkDescriptorSetLayoutBinding
    convert_to_vk_binding(const reflection::binding& b, VkShaderStageFlags stage);

    static void
    convert_to_vk_push_constants(const reflection::push_constants& pc,
                                 VkShaderStageFlags stage,
                                 VkPushConstantRange& range);
};
}  // namespace kryga::render

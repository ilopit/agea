#pragma once

// Re-export shader_reflection_utils from shader_system
#include <shader_system/shader_reflection.h>

#include "vulkan_render/types/vulkan_shader_data.h"

namespace kryga
{
namespace render
{

struct vulkan_descriptor_set_layout_data;

// Extension functions that require vulkan_render types
struct vulkan_shader_reflection_utils
{
    static void
    convert_to_ds_layout_data(const reflection::descriptor_set& ref_set,
                              VkShaderStageFlags stage,
                              vulkan_descriptor_set_layout_data& layout);

    // Retrieves pre-built reflection from shader_module_data
    // Reflection is now built during compilation in shader_compiler
    static bool
    build_shader_reflection(const std::shared_ptr<shader_module_data>& sd,
                            reflection::shader_reflection& sr)
    {
        sr = sd->get_reflection();
        return true;
    }
};

}  // namespace render
}  // namespace kryga
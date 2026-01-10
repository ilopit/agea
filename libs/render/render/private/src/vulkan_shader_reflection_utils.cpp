#include "vulkan_render/shader_reflection_utils.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

namespace kryga
{
namespace render
{

void
vulkan_shader_reflection_utils::convert_to_ds_layout_data(const reflection::descriptor_set& ref_set,
                                                          VkShaderStageFlags stage,
                                                          vulkan_descriptor_set_layout_data& layout)
{
    layout.set_idx = ref_set.location;
    for (auto& b : ref_set.bindigns)
    {
        layout.bindings.push_back(shader_reflection_utils::convert_to_vk_binding(b, stage));
    }
}

}  // namespace render
}  // namespace kryga

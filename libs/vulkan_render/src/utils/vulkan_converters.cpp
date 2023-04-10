#include "vulkan_render/utils/vulkan_converters.h"

#include <utils/check.h>

namespace agea::render::vk_utils
{
VkFormat
convert_to_vk_format(render::gpu_type::id t)
{
    switch (t)
    {
    case agea::render::gpu_type::g_vec2:
        return VK_FORMAT_R32G32_SFLOAT;
    case agea::render::gpu_type::g_vec3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case agea::render::gpu_type::g_vec4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case agea::render::gpu_type::g_color:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case agea::render::gpu_type::g_nan:
    default:
        AGEA_never("Should never happens!");
    }

    return VK_FORMAT_UNDEFINED;
}

}  // namespace agea::render::vk_utils

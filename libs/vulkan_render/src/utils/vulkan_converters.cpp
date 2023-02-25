#include "vulkan_render/utils/vulkan_converters.h"

#include <utils/check.h>

namespace agea::render::vk_utils
{
VkFormat
convert_to_vk_format(utils::agea_type::id t)
{
    switch (t)
    {
    case agea::utils::agea_type::id::t_vec2:
        return VK_FORMAT_R32G32_SFLOAT;
    case agea::utils::agea_type::id::t_vec3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case agea::utils::agea_type::id::t_vec4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case agea::utils::agea_type::id::t_color:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case agea::utils::agea_type::id::t_nan:
    default:
        AGEA_never("Should never happens!");
    }

    return VK_FORMAT_UNDEFINED;
}

}  // namespace agea::render::vk_utils

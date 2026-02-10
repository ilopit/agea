#include "vulkan_render/utils/vulkan_converters.h"

#include <utils/check.h>

namespace kryga::render::vk_utils
{
VkFormat
convert_to_vk_format(render::gpu_type::id t)
{
    switch (t)
    {
    case kryga::render::gpu_type::g_vec2:
        return VK_FORMAT_R32G32_SFLOAT;
    case kryga::render::gpu_type::g_vec3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case kryga::render::gpu_type::g_vec4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case kryga::render::gpu_type::g_color:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case kryga::render::gpu_type::g_uvec2:
        return VK_FORMAT_R32G32_UINT;
    case kryga::render::gpu_type::g_uvec3:
        return VK_FORMAT_R32G32B32_UINT;
    case kryga::render::gpu_type::g_uvec4:
        return VK_FORMAT_R32G32B32A32_UINT;
    case kryga::render::gpu_type::nan:
    default:
        KRG_never("Should never happens!");
    }

    return VK_FORMAT_UNDEFINED;
}

}  // namespace kryga::render::vk_utils

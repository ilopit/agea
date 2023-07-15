#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_render/types/vulkan_gpu_types.h"

namespace agea
{
namespace render
{
namespace vk_utils
{
VkFormat
convert_to_vk_format(render::gpu_type::id t);

}
}  // namespace render
}  // namespace agea
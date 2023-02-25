#pragma once

#include <utils/agea_types.h>

#include <vulkan/vulkan.h>

namespace agea
{
namespace render
{
namespace vk_utils
{
VkFormat
convert_to_vk_format(agea::utils::agea_type::id t);

}
}  // namespace render
}  // namespace agea
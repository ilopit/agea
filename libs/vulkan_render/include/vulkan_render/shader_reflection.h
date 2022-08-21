#pragma once

#include <vulkan_render_types/vulkan_render_fwds.h>
#include <vulkan_render_types/vulkan_shader_effect_data.h>

namespace agea
{
namespace render
{
class render_device;

bool
build_shader_reflection(render_device* device, shader_effect_data& se);

}  // namespace render
}  // namespace agea
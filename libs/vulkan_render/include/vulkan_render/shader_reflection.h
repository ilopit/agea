#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

namespace agea
{
namespace render
{
class render_device;

bool
build_shader_reflection(render_device* device,
                        reflection::shader_reflection& sr,
                        std::shared_ptr<shader_data>& sd);

}  // namespace render
}  // namespace agea
#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"

#include <utils/buffer.h>
#include <utils/id.h>
#include <utils/path.h>

#include <vulkan/vulkan.h>

#include <memory>

namespace agea
{
namespace render
{
namespace vulkan_shader_loader
{
bool
create_shader_effect_pipeline_layout(shader_effect_data& se);

bool
update_shader_effect(shader_effect_data& se_data,
                     const shader_effect_create_info& info,
                     std::shared_ptr<render::shader_effect_data>& old_se_data);

bool
create_shader_effect(shader_effect_data& se_data, const shader_effect_create_info& info);

bool
create_shader_effect(shader_effect_data& se_data,
                     std::shared_ptr<shader_data>& vert_module,
                     std::shared_ptr<shader_data>& frag_module,
                     const shader_effect_create_info& info);

}  // namespace vulkan_shader_loader
}  // namespace render
}  // namespace agea
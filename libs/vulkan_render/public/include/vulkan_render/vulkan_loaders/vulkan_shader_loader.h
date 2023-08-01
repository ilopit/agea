#pragma once

#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"

#include <error_handling/error_handling.h>

#include <utils/buffer.h>
#include <utils/id.h>
#include <utils/path.h>

#include <memory>

namespace agea
{
namespace render
{
namespace vulkan_shader_loader
{
bool
create_shader_effect_pipeline_layout(shader_effect_data& se);

result_code
update_shader_effect(shader_effect_data& se_data,
                     const shader_effect_create_info& info,
                     std::shared_ptr<render::shader_effect_data>& old_se_data);

result_code
create_shader_effect(shader_effect_data& se_data, const shader_effect_create_info& info);

result_code
create_shader_effect(shader_effect_data& se_data,
                     std::shared_ptr<shader_module_data>& vert_module,
                     std::shared_ptr<shader_module_data>& frag_module,
                     const shader_effect_create_info& info);

}  // namespace vulkan_shader_loader
}  // namespace render
}  // namespace agea
#pragma once

#include <vulkan_render_types/vulkan_render_fwds.h>
#include <vulkan_render_types/vulkan_gpu_types.h>

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
create_shader_effect_pipeline(shader_effect_data& se);

bool
update_shader_effect(shader_effect_data& se_data,
                     agea::utils::buffer& vert_buff,
                     bool is_vert_binary,
                     agea::utils::buffer& frag_buff,
                     bool is_frag_binary,
                     bool is_wire,
                     bool enable_alpha,
                     VkRenderPass render_pass,
                     std::shared_ptr<render::shader_effect_data>& old_se_data);

bool
create_shader_effect(shader_effect_data& se_data,
                     agea::utils::buffer& vert_buff,
                     bool is_vert_binary,
                     agea::utils::buffer& frag_buff,
                     bool is_frag_binary,
                     bool is_vire,
                     bool enable_alpha,
                     VkRenderPass render_pass);

bool
create_shader_effect(shader_effect_data& se_data,
                     std::shared_ptr<shader_data>& vert_module,
                     std::shared_ptr<shader_data>& frag_module,
                     bool is_wire,
                     bool enable_alpha,
                     VkRenderPass render_pass);

}  // namespace vulkan_shader_loader
}  // namespace render
}  // namespace agea
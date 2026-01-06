#pragma once

#include "vulkan_render/types/vulkan_compute_shader_data.h"

#include <error_handling/error_handling.h>

#include <utils/buffer.h>
#include <utils/id.h>

#include <memory>

namespace kryga
{
namespace render
{
namespace vulkan_compute_shader_loader
{

// Create a compute pipeline from shader source
result_code
create_compute_shader(compute_shader_data& cs_data, const kryga::utils::buffer& shader_buffer);

// Create pipeline layout from reflection data
bool
create_compute_pipeline_layout(compute_shader_data& cs);

}  // namespace vulkan_compute_shader_loader
}  // namespace render
}  // namespace kryga

#pragma once

#include <error_handling/error_handling.h>

#include <utils/id.h>

namespace kryga
{
namespace render
{

class compute_shader_data;
struct compute_shader_create_info;

namespace vulkan_compute_shader_loader
{

// Create a compute pipeline from shader source
result_code
create_compute_shader(compute_shader_data& cs_data, const compute_shader_create_info& info);

// Create pipeline layout from reflection data
bool
create_compute_pipeline_layout(compute_shader_data& cs);

}  // namespace vulkan_compute_shader_loader
}  // namespace render
}  // namespace kryga

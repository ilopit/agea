#pragma once

#include <vulkan_render_types/vulkan_gpu_types.h>

#include <utils/path.h>
#include <utils/buffer.h>

#include <string>

namespace agea
{
namespace asset_importer
{
namespace mesh_importer
{
bool
extract_mesh_data_from_3do(const utils::path& obj_path,
                           utils::buffer_view<render::gpu_vertex_data> vertices,
                           utils::buffer_view<render::gpu_index_data> indices);

}
}  // namespace asset_importer

}  // namespace agea
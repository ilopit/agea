#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <utils/path.h>
#include <utils/buffer.h>

#include <string>

namespace kryga
{
namespace asset_importer
{
namespace mesh_importer
{
bool
extract_mesh_data_from_3do(const utils::path& obj_path,
                           utils::buffer_view<gpu::vertex_data> vertices,
                           utils::buffer_view<gpu::uint> indices);

}
}  // namespace asset_importer

}  // namespace kryga
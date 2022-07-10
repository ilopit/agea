#pragma once

#include "vulkan_render/vulkan_render_fwds.h"

#include <utils/path.h>

#include <string>

namespace agea
{
namespace render
{
struct vulkan_mesh_data_loader
{
    static bool
    load_from_obj(const utils::path& obj_path, mesh_data& md);

    static bool
    load_from_amsh(const utils::path& index_file, const utils::path& verteces_file, mesh_data& md);
};
}  // namespace render
}  // namespace agea

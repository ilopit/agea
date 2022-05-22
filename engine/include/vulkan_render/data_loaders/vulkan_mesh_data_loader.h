#pragma once

#include "vulkan_render/vulkan_render_fwds.h"

#include "core/agea_minimal.h"

#include <string>

namespace agea
{
namespace render
{
struct vulkan_mesh_data_loader
{
    static bool
    load_from_obj(const std::string& name, mesh_data& md);

    static bool
    load_from_amsh(const std::string& index_file, const std::string& verteces_file, mesh_data& md);
};
}  // namespace render
}  // namespace agea

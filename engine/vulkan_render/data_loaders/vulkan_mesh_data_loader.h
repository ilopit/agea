#pragma once

#include "vulkan_render/vulkan_render_fwds.h"

#include "agea_minimal.h"

#include <string>

namespace agea
{
namespace render
{
struct vulkan_mesh_data_loader
{
    bool load_from_obj(const std::string& name, mesh_data& md);
};
}  // namespace render
}  // namespace agea

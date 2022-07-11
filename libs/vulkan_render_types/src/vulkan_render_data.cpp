#pragma once

#include <vulkan_render_types/vulkan_render_data.h>

#include "vulkan_render_types/vulkan_mesh_data.h"
#include "vulkan_render_types/vulkan_material_data.h"

namespace agea
{
namespace render
{
std::string
render_data::gen_render_data_id() const
{
    return material->id.str() + "::" + mesh->id().str();
}

bool
render_data::empty()
{
    return !mesh || !material;
}

}  // namespace render
}  // namespace agea

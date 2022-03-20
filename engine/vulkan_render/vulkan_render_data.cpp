#pragma once

#include <vulkan_render/vulkan_render_data.h>

#include "vulkan_render/vulkan_mesh_data.h"
#include "vulkan_render/vulkan_material_data.h"

namespace agea
{

std::string
render_data::id() const
{
    return material->id + "::" + mesh->id();
}

}  // namespace agea

#pragma once

#include "agea_minimal.h"

#include "vulkan_render/vulkan_render_fwds.h"

#include <string>

namespace agea
{
struct render_data
{
    std::string id() const;

    bool visible = true;
    render::mesh_data* mesh = nullptr;
    render::material_data* material = nullptr;

    glm::mat4 transform_matrix;
};

}  // namespace agea

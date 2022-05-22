#pragma once

#include "core/agea_minimal.h"

#include "vulkan_render/vulkan_render_fwds.h"

#include <string>

namespace agea
{
namespace render
{
struct render_data
{
    std::string
    gen_render_data_id() const;

    bool
    empty();

    render::mesh_data* mesh = nullptr;
    render::material_data* material = nullptr;

    glm::mat4 transform_matrix;

    bool visible = true;
    bool rendarable = true;
};
};  // namespace render
}  // namespace agea

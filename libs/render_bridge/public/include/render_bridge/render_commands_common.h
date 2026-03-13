#pragma once

#include "render_bridge/render_command.h"
#include "render_bridge/render_command_processor.h"

#include <utils/id.h>

#include <glm/glm.hpp>

namespace kryga
{

struct update_transform_cmd : render_cmd::render_command_base
{
    utils::id id;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    float bounding_radius = 0.0f;

    void
    execute(render_cmd::render_exec_context& ctx) override;
};

}  // namespace kryga

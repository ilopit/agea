#pragma once

#include "render_bridge/render_command.h"

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
    glm::vec3 bounding_sphere_center{0.0f};
    float bounding_radius = 0.0f;

    void
    execute(render_cmd::render_exec_context& ctx) override;
};

// Set an object's outline flag and re-bucket it between the default and outline
// render queues. Runs on the render thread (those queues are render-owned and
// iterated during draw) — editor selection must NOT touch them directly.
struct set_outline_cmd : render_cmd::render_command_base
{
    utils::id id;
    bool outlined = false;

    void
    execute(render_cmd::render_exec_context& ctx) override;
};

}  // namespace kryga

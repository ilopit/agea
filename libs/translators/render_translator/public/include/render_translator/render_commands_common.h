#pragma once

#include "render_translator/render_command.h"

#include <utils/id.h>

#include <render_types/render_handle.h>

#include <glm/glm.hpp>

namespace kryga
{

struct update_transform_cmd : render_cmd::render_command_base
{
    void
    execute(render_cmd::render_exec_context& ctx) override;

    render::types::render_object_handle obj_handle;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 bounding_sphere_center{0.0f};
    float bounding_radius = 0.0f;
};

// Set an object's outline flag and re-bucket it between the default and outline
// render queues. Runs on the render thread (those queues are render-owned and
// iterated during draw) — editor selection must NOT touch them directly.
struct set_outline_cmd : render_cmd::render_command_base
{
    void
    execute(render_cmd::render_exec_context& ctx) override;

    render::types::render_object_handle obj_handle;
    bool outlined = false;
};

}  // namespace kryga

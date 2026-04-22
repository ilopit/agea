#pragma once

#include "render_bridge/render_command.h"

#include <utils/id.h>
#include <utils/slot_handle.h>

#include <glm/glm.hpp>

namespace kryga
{

namespace render
{
class vulkan_render_data;
}

struct update_transform_cmd : render_cmd::render_command_base
{
    utils::slot_handle<render::vulkan_render_data> object_handle;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    float bounding_radius = 0.0f;

    void
    execute(render_cmd::render_exec_context& ctx) override;
};

}  // namespace kryga

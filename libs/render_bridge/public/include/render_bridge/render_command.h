#pragma once

#include <utils/cmd_arena.h>

namespace kryga
{

namespace render
{
class vulkan_render;
class vulkan_render_loader;
}  // namespace render

namespace render_cmd
{

// Re-export arena from utils for backward compatibility.
using cmd_arena = utils::cmd_arena;

struct render_exec_context
{
    render::vulkan_render& vr;
    render::vulkan_render_loader& loader;
};

struct render_command_base
{
    virtual ~render_command_base() = default;
    virtual void execute(render_exec_context& ctx) = 0;
};

}  // namespace render_cmd
}  // namespace kryga

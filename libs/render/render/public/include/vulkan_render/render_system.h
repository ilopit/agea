#pragma once

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/input_queue.h>

#include <global_state/system.h>

namespace kryga::render
{

class render_system : public gs::system
{
public:
    std::string_view
    name() const override
    {
        return "render";
    }
    std::span<const std::string_view>
    deps() const override
    {
        return {};
    }

    render_device device;
    vulkan_render renderer;
    vulkan_render_loader loader;

    // The render system's command input — main thread produces render commands
    // into the active parity slot, the render thread drains the other slot.
    input_queue input_queue;
};

}  // namespace kryga::render

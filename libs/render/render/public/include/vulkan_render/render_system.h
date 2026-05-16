#pragma once

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>

#include <global_state/system.h>

namespace kryga::render
{

class render_system : public gs::system
{
public:
    std::string_view system_name() const override { return "render"; }
    std::span<const std::string_view> system_deps() const override
    {
        static constexpr std::string_view d[] = {"model"};
        return d;
    }

    render_device device;
    vulkan_render renderer;
    vulkan_render_loader loader;
};

}  // namespace kryga::render

#pragma once

#include <utils/combined_pool.h>

#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/types/vulkan_light_data.h>

namespace agea
{
namespace render
{

class vulkan_render_data;

class render_cache
{
public:
    render_cache();
    ~render_cache();

    utils::combined_pool<vulkan_render_data> objects;
    utils::combined_pool<vulkan_directional_light_data> dir_lights;
    utils::combined_pool<vulkan_spot_light_data> spot_lights;
    utils::combined_pool<vulkan_point_light_data> point_lights;
};

}  // namespace render
}  // namespace agea
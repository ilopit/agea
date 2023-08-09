#pragma once

#include <utils/combined_pool.h>

#include <vulkan_render/types/vulkan_render_data.h>

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

    utils::combined_pool<vulkan_render_data> m_objects;
};

}  // namespace render
}  // namespace agea
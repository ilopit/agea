#pragma once
#include "model/rendering/renderable.h"

#include "utils/defines_utils.h"

#include "vulkan_render/vulkan_render_data.h"
#include "core/vk_engine.h"

namespace agea
{
namespace model
{

renderable::renderable()
    : m_render_data(std::make_unique<render::render_data>())
{
}

renderable::~renderable()
{
}

bool
renderable::mark_dirty()
{
    if (!m_dirty)
    {
        glob::engine::get()->qs().add_to_dirty_queue(this);
        m_dirty = true;
    }

    return true;
}

}  // namespace model
}  // namespace agea
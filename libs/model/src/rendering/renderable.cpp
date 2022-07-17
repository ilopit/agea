#pragma once
#include "model/rendering/renderable.h"

#include <model_global_api/render_api.h>

#include <utils/defines_utils.h>
#include <vulkan_render_types/vulkan_render_data.h>

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
        glob::model_render_api::get()->invalidate(this);
        m_dirty = true;
    }

    return true;
}

}  // namespace model
}  // namespace agea
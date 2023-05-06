#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

#include <glm_unofficial/glm.h>

#include <utils/id.h>

#include <string>

namespace agea
{
namespace render
{
class ligh_data
{
public:
    ligh_data(const ::agea::utils::id& id)
        : m_id(id)
    {
    }

    const utils::id&
    get_id() const
    {
        return m_id;
    }

    glm::vec4 obj_pos;

private:
    ::agea::utils::id m_id;
};
};  // namespace render
}  // namespace agea

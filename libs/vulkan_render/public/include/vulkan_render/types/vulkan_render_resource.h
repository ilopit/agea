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
class vulkan_render_resource
{
public:
    vulkan_render_resource(const utils::id& id, gpu_data_index_type idx)
        : m_id(id)
        , m_idx(idx)
    {
    }

    ~vulkan_render_resource()
    {
        m_id.invalidate();
        m_idx = INVALID_GPU_INDEX;
    }

    const utils::id&
    id() const
    {
        return m_id;
    }

    gpu_data_index_type
    slot() const
    {
        return m_idx;
    }

protected:
    utils::id m_id;
    gpu_data_index_type m_idx;
};
}  // namespace render
}  // namespace agea
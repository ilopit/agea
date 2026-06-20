#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

#include <glm_unofficial/glm.h>

#include <utils/id.h>

#include <string>

namespace kryga
{
namespace render
{
class vulkan_render_resource
{
public:
    // Default-constructs to an empty/invalid slot. Needed so resources can live
    // by value in a laned_storage (the render-object pool), whose growth
    // pre-constructs slots before they're populated via create_object.
    vulkan_render_resource()
        : m_idx(INVALID_GPU_INDEX)
    {
    }

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

    bool
    is_pending_release() const
    {
        return m_pending_release;
    }

    void
    mark_pending_release()
    {
        m_pending_release = true;
    }

protected:
    utils::id m_id;
    gpu_data_index_type m_idx;
    bool m_pending_release = false;
};

template <typename gpu_data_type>
class generic_vulkan_render_resource : public vulkan_render_resource
{
public:
    gpu_data_type gpu_data;
};

}  // namespace render
}  // namespace kryga
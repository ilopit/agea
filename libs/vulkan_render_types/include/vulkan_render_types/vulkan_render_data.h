#pragma once

#include "vulkan_render_types/vulkan_render_fwds.h"
#include "vulkan_render_types/vulkan_gpu_types.h"

#include <glm_unofficial/glm.h>

#include <utils/id.h>

#include <string>

namespace agea
{
namespace render
{
class object_data
{
public:
    object_data(const ::agea::utils::id& id, gpu_data_index_type idx)
        : m_id(id)
        , m_gpu_index(idx)
    {
    }

    const utils::id&
    get_id() const
    {
        return m_id;
    }

    gpu_data_index_type
    gpu_index()
    {
        return m_gpu_index;
    }

    gpu_object_data gpu_data;

    render::mesh_data* mesh = nullptr;
    render::material_data* material = nullptr;
    bool visible = true;
    bool rendarable = true;
    float distance_to_camera = 0.f;

    std::string queue_id;

private:
    ::agea::utils::id m_id;

    gpu_data_index_type m_gpu_index = INVALID_GPU_MATERIAL_DATA_INDEX;
};
};  // namespace render
}  // namespace agea

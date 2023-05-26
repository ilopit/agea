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

union gpu_light
{
    gpu_directional_light_data directional;
    gpu_point_light_data point;
    gpu_spot_light_data spot;
};

enum class light_type
{
    nan,
    directional_light_data,
    point_light_data,
    spot_light_data
};

class light_data
{
public:
    light_data(const ::agea::utils::id& id, light_type lt)
        : m_id(id)
        , m_type(lt)
    {
    }

    const utils::id&
    get_id() const
    {
        return m_id;
    }

    light_type m_type;
    gpu_light m_data;
    gpu_data_index_type m_gpu_id = INVALID_GPU_INDEX;

private:
    ::agea::utils::id m_id;
};

};  // namespace render
}  // namespace agea

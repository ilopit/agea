#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_render_resource.h"

#include <utils/id.h>

#include <glm_unofficial/glm.h>

#include <string>

namespace agea
{
namespace render
{
class vulkan_directional_light_data : public vulkan_render_resource
{
public:
    vulkan_directional_light_data(const utils::id& id, gpu_data_index_type idx)
        : vulkan_render_resource(id, idx)
    {
    }
    gpu_directional_light_data gpu_data;
};

class vulkan_point_light_data : public vulkan_render_resource
{
public:
    vulkan_point_light_data(const utils::id& id, gpu_data_index_type idx)
        : vulkan_render_resource(id, idx)
    {
    }
    gpu_point_light_data gpu_data;
};

class vulkan_spot_light_data : public vulkan_render_resource
{
public:
    vulkan_spot_light_data(const utils::id& id, gpu_data_index_type idx)
        : vulkan_render_resource(id, idx)
    {
    }
    gpu_spot_light_data gpu_data;
};

};  // namespace render
}  // namespace agea

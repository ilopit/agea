#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_render_resource.h"
#include "render_utils/light_grid.h"

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
    vulkan_directional_light_data(const utils::id& id, gpu::uint idx)
        : vulkan_render_resource(id, idx)
    {
    }
    gpu::directional_light_data gpu_data;
};

// Unified local light (point + spot combined)
class vulkan_universal_light_data : public vulkan_render_resource
{
public:
    vulkan_universal_light_data(const utils::id& id, gpu_data_index_type idx, light_type type)
        : vulkan_render_resource(id, idx)
    {
        gpu_data.type = static_cast<uint32_t>(type);
        // Set defaults for point light (spot fields unused)
        gpu_data.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        gpu_data.cut_off = -1.0f;  // indicates point light in shader
        gpu_data.outer_cut_off = -1.0f;
    }

    gpu::universal_light_data gpu_data;
};

}  // namespace render
}  // namespace agea

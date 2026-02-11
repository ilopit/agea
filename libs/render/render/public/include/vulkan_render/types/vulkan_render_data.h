#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_render_resource.h"

#include <glm_unofficial/glm.h>

#include <utils/id.h>

#include <string>

namespace kryga
{
namespace render
{
class vulkan_render_data : public vulkan_render_resource
{
public:
    vulkan_render_data() = default;

    vulkan_render_data(const ::kryga::utils::id& id, gpu_data_index_type idx)
        : vulkan_render_resource(id, idx)
    {
    }

    gpu::object_data gpu_data;
    render::mesh_data* mesh = nullptr;
    render::material_data* material = nullptr;

    bool visible = true;
    bool renderable = true;
    float distance_to_camera = 0.f;
    float bounding_radius = 1.0f;  // For light grid culling
    bool outlined = false;

    // Skeletal animation state
    uint32_t bone_offset = 0;  // offset into global bone matrices SSBO
    uint32_t bone_count = 0;   // number of bones (0 = not animated)

    std::string queue_id;
};
};  // namespace render
}  // namespace kryga

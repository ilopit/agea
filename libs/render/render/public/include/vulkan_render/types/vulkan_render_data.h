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

// Object layer flag bits — must match core::object_layer_flags layout
constexpr uint32_t LAYER_VISIBLE = 1u << 0;
constexpr uint32_t LAYER_EDITOR_ONLY = 1u << 1;
constexpr uint32_t LAYER_CAST_SHADOWS = 1u << 2;
constexpr uint32_t LAYER_RECEIVE_LIGHT = 1u << 3;
constexpr uint32_t LAYER_CONTRIBUTE_GI = 1u << 4;
constexpr uint32_t LAYER_STATIC_OBJECT = 1u << 5;

constexpr uint32_t LAYER_DEFAULT_STATIC =
    LAYER_VISIBLE | LAYER_CAST_SHADOWS | LAYER_RECEIVE_LIGHT | LAYER_CONTRIBUTE_GI | LAYER_STATIC_OBJECT;

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

    uint32_t layer_flags = LAYER_DEFAULT_STATIC;
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

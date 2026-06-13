#pragma once

#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/utils/vulkan_buffer.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <render_types/render_handle.h>

#include <glm/vec3.hpp>

namespace kryga
{
namespace render
{
struct vertex_input_description
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

vertex_input_description
convert_to_vertex_input_description(kryga::utils::dynobj_layout& dol);

class mesh_data
{
public:
    // Empty slot value — slot_storage pre-constructs slots on growth and
    // reset() assigns this over the old payload.
    mesh_data() = default;

    mesh_data(const ::kryga::utils::id& id)
        : m_id(id)
    {
    }
    ~mesh_data();

    mesh_data(mesh_data&& other) noexcept = default;
    mesh_data&
    operator=(mesh_data&& other) noexcept = default;

    uint32_t
    vertices_size()
    {
        return m_vertices_size;
    }

    uint32_t
    indices_size()
    {
        return m_indices_size;
    }

    bool
    has_indices()
    {
        return m_indices_size;
    }

    const ::kryga::utils::id&
    get_id()
    {
        return m_id;
    }

    // The pool slot this mesh lives in — set by populate. Lets a holder of the
    // mesh_data* recover its handle (e.g. for destroy) without an id->handle map.
    render::types::mesh_handle
    render_handle() const
    {
        return m_render_handle;
    }

    void
    set_render_handle(render::types::mesh_handle h)
    {
        m_render_handle = h;
    }

    uint32_t m_vertices_size = 0U;
    uint32_t m_indices_size = 0U;
    // Tight sphere around the geometry centroid (NOT the local origin).
    // m_local_centroid + m_bounding_radius together define the sphere in
    // local space — transform the center, scale the radius for world bounds.
    glm::vec3 m_local_centroid{0.0f};
    float m_bounding_radius = 0.0f;
    bool m_is_skinned = false;

    vk_utils::vulkan_buffer m_vertex_buffer;
    vk_utils::vulkan_buffer m_index_buffer;

private:
    ::kryga::utils::id m_id;
    render::types::mesh_handle m_render_handle{};
};
}  // namespace render

}  // namespace kryga

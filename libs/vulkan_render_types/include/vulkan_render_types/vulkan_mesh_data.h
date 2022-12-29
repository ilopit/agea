#pragma once

#include "vulkan_render_types/vulkan_types.h"
#include "vulkan_render_types/vulkan_gpu_types.h"

#include <utils/id.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace render
{
struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

VertexInputDescription
get_vertex_description();

class mesh_data
{
public:
    mesh_data(const ::agea::utils::id& id)
        : m_id(id)
    {
    }
    ~mesh_data();

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

    const ::agea::utils::id&
    get_id()
    {
        return m_id;
    }

    uint32_t m_vertices_size = 0U;
    uint32_t m_indices_size = 0U;

    allocated_buffer m_vertexBuffer;
    allocated_buffer m_indexBuffer;

private:
    ::agea::utils::id m_id;
};
}  // namespace render

}  // namespace agea

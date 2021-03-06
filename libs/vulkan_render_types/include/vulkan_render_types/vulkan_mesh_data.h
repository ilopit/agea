#pragma once

#include "vulkan_render_types/vulkan_types.h"
#include "vulkan_render_types/vulkan_gpu_types.h"

#include <utils/id.h>
#include <glm_unofficial/glm.h>

#include <vector>
#include <string>

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

struct mesh_data
{
    uint32_t
    vertices_size()
    {
        return (uint32_t)m_vertices.size();
    }

    uint32_t
    indices_size()
    {
        return (uint32_t)m_indices.size();
    }

    bool
    has_indices()
    {
        return !m_indices.empty();
    }

    const agea::utils::id&
    id()
    {
        return m_id;
    }

    agea::utils::id m_id;

    std::vector<vertex_data> m_vertices;
    std::vector<uint32_t> m_indices;

    allocated_buffer m_vertexBuffer;
    allocated_buffer m_indexBuffer;

    ~mesh_data();
};
}  // namespace render

}  // namespace agea

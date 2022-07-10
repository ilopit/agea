#pragma once

#include "vulkan_render/vulkan_types.h"

#include "utils/id.h"

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

struct vertex_data
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static VertexInputDescription
    get_vertex_description();
};

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

    const utils::id&
    id()
    {
        return m_id;
    }

    utils::id m_id;

    std::vector<vertex_data> m_vertices;
    std::vector<uint32_t> m_indices;

    allocated_buffer m_vertexBuffer;
    allocated_buffer m_indexBuffer;

    ~mesh_data();
};
}  // namespace render

}  // namespace agea

#pragma once

#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/utils/vulkan_buffer.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

namespace agea
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
convert_to_vertex_input_description(agea::utils::dynamic_object_layout& dol);

class mesh_data
{
public:
    mesh_data(const ::agea::utils::id& id)
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

    const ::agea::utils::id&
    get_id()
    {
        return m_id;
    }

    uint32_t m_vertices_size = 0U;
    uint32_t m_indices_size = 0U;

    vk_utils::vulkan_buffer m_vertex_buffer;
    vk_utils::vulkan_buffer m_index_buffer;

private:
    ::agea::utils::id m_id;
};
}  // namespace render

}  // namespace agea

#include "vulkan_render/vulkan_mesh_data.h"

#include "vulkan_render/render_device.h"

#include "utils/file_utils.h"

#include <iostream>
#include <fstream>

namespace agea
{
namespace render
{
VertexInputDescription
vertex_data::get_vertex_description()
{
    VertexInputDescription description;

    // we will have just 1 vertex buffer binding, with a per-vertex rate
    VkVertexInputBindingDescription main_binding = {};
    main_binding.binding = 0;
    main_binding.stride = sizeof(vertex_data);
    main_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    description.bindings.push_back(main_binding);

    // Position will be stored at Location 0
    VkVertexInputAttributeDescription position_attribute = {};
    position_attribute.binding = 0;
    position_attribute.location = 0;
    position_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    position_attribute.offset = offsetof(vertex_data, position);

    // Normal will be stored at Location 1
    VkVertexInputAttributeDescription normal_attribute = {};
    normal_attribute.binding = 0;
    normal_attribute.location = 1;
    normal_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal_attribute.offset = offsetof(vertex_data, normal);

    // Position will be stored at Location 2
    VkVertexInputAttributeDescription color_attribute = {};
    color_attribute.binding = 0;
    color_attribute.location = 2;
    color_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    color_attribute.offset = offsetof(vertex_data, color);

    // UV will be stored at Location 2
    VkVertexInputAttributeDescription uv_attribute = {};
    uv_attribute.binding = 0;
    uv_attribute.location = 3;
    uv_attribute.format = VK_FORMAT_R32G32_SFLOAT;
    uv_attribute.offset = offsetof(vertex_data, uv);

    description.attributes.push_back(position_attribute);
    description.attributes.push_back(normal_attribute);
    description.attributes.push_back(color_attribute);
    description.attributes.push_back(uv_attribute);
    return description;
}

mesh_data::~mesh_data()
{
}

}  // namespace render

}  // namespace agea

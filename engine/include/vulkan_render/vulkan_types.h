#pragma once

#include "core/agea_minimal.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>

namespace agea
{
namespace render
{
struct allocated_buffer
{
    allocated_buffer();

    ~allocated_buffer();

    allocated_buffer(VkBuffer b, VmaAllocation a);

    allocated_buffer(const allocated_buffer&) = delete;
    allocated_buffer&
    operator=(const allocated_buffer&) = delete;

    allocated_buffer(allocated_buffer&& other) noexcept;
    allocated_buffer&
    operator=(allocated_buffer&& other) noexcept;

    void
    clear();

    static allocated_buffer
    create(VkBufferCreateInfo bci, VmaAllocationCreateInfo vaci);

    VkBuffer&
    buffer()
    {
        return m_buffer;
    }

    VmaAllocation
    allocation()
    {
        return m_allocation;
    }

private:
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
};

struct allocated_image
{
    allocated_image();

    allocated_image(VkImage i, VmaAllocation a);

    allocated_image(const allocated_image&) = delete;
    allocated_image&
    operator=(const allocated_image&) = delete;

    allocated_image(allocated_image&& other) noexcept;
    allocated_image&
    operator=(allocated_image&& other) noexcept;

    ~allocated_image();

    void
    clear();

    VkImage m_image;
    VmaAllocation m_allocation;

    int mipLevels;
};

class pipeline_builder
{
public:
    VkPipeline
    build_pipeline(VkDevice device, VkRenderPass pass);

    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
    VkPipelineVertexInputStateCreateInfo m_vertex_input_info;
    VkPipelineInputAssemblyStateCreateInfo m_input_assembly;
    VkViewport m_viewport;
    VkRect2D m_scissor;
    VkPipelineRasterizationStateCreateInfo m_rasterizer;
    VkPipelineColorBlendAttachmentState m_color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo m_multisampling;
    VkPipelineLayout m_pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo m_depth_stencil;
};

struct mesh_push_constants
{
    glm::vec4 data;
    glm::mat4 render_matrix;
};

struct gpu_camera_data
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 pos;
};

struct gpu_scene_data
{
    glm::vec4 lights[4];
    glm::vec4 fog_color;      // w is for exponent
    glm::vec4 fog_distances;  // x for min, y for max, zw unused.
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;  // w for sun power
    glm::vec4 sunlight_color;
};

struct gpu_object_data
{
    glm::mat4 model_matrix;
    glm::vec3 obj_pos;
    float dummy;
};

}  // namespace render
}  // namespace agea

#define VK_CHECK(x)                                                     \
    do                                                                  \
    {                                                                   \
        VkResult err = x;                                               \
        if (err)                                                        \
        {                                                               \
            std::cout << "Detected Vulkan error: " << err << std::endl; \
            abort();                                                    \
        }                                                               \
    } while (0)

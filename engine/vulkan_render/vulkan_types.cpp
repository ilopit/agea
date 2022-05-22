#include "vulkan_render/vulkan_types.h"

#include "vulkan_render/render_device.h"

namespace agea
{
namespace render
{

allocated_buffer&
allocated_buffer::operator=(allocated_buffer&& other) noexcept
{
    if (this != &other)
    {
        clear();
    }
    m_buffer = other.m_buffer;
    m_allocation = other.m_allocation;

    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;

    return *this;
}

allocated_buffer::allocated_buffer()
    : m_buffer(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
{
}

allocated_buffer::allocated_buffer(VkBuffer b, VmaAllocation a)
    : m_buffer(b)
    , m_allocation(a)
{
}

allocated_buffer::allocated_buffer(allocated_buffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
}

void
allocated_buffer::clear()
{
    auto device = glob::render_device::get();

    vmaDestroyBuffer(device->allocator(), m_buffer, m_allocation);

    m_buffer = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
}

allocated_buffer
allocated_buffer::create(VkBufferCreateInfo bci, VmaAllocationCreateInfo vaci)
{
    auto device = glob::render_device::get();

    VkBuffer buffer;
    VmaAllocation allocation;

    vmaCreateBuffer(device->allocator(), &bci, &vaci, &buffer, &allocation, nullptr);

    return allocated_buffer{buffer, allocation};
}

allocated_buffer::~allocated_buffer()
{
    clear();
}

allocated_image::allocated_image(VkImage b, VmaAllocation a)
    : m_image(b)
    , m_allocation(a)
    , mipLevels(0)
{
}

allocated_image::allocated_image(allocated_image&& other) noexcept
    : m_image(other.m_image)
    , m_allocation(other.m_allocation)
    , mipLevels(other.mipLevels)
{
    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.mipLevels = 0;
}

allocated_image::allocated_image()
    : m_image(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
    , mipLevels(0)
{
}

allocated_image&
allocated_image::operator=(allocated_image&& other) noexcept
{
    if (this != &other)
    {
        clear();
    }
    m_image = other.m_image;
    m_allocation = other.m_allocation;
    mipLevels = other.mipLevels;

    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.mipLevels = 0;

    return *this;
}

allocated_image::~allocated_image()
{
    clear();
}

void
allocated_image::clear()
{
    if (m_image == VK_NULL_HANDLE)
    {
        return;
    }

    auto device = glob::render_device::get();

    if (!device)
    {
        AGEA_never("never");
    }

    vmaDestroyImage(device->allocator(), m_image, m_allocation);

    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
}

VkPipeline
pipeline_builder::build_pipeline(VkDevice device, VkRenderPass pass)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo vs = {};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.pNext = nullptr;

    vs.viewportCount = 1;
    vs.pViewports = &m_viewport;
    vs.scissorCount = 1;
    vs.pScissors = &m_scissor;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.pNext = nullptr;

    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_COPY;
    cb.attachmentCount = 1;
    cb.pAttachments = &m_color_blend_attachment;

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one to create the
    // pipeline
    VkGraphicsPipelineCreateInfo pi = {};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.pNext = nullptr;

    pi.stageCount = (uint32_t)m_shader_stages.size();
    pi.pStages = m_shader_stages.data();
    pi.pVertexInputState = &m_vertex_input_info;
    pi.pInputAssemblyState = &m_input_assembly;
    pi.pViewportState = &vs;
    pi.pRasterizationState = &m_rasterizer;
    pi.pMultisampleState = &m_multisampling;
    pi.pColorBlendState = &cb;
    pi.pDepthStencilState = &m_depth_stencil;
    pi.layout = m_pipelineLayout;
    pi.renderPass = pass;
    pi.subpass = 0;
    pi.basePipelineHandle = VK_NULL_HANDLE;

    // its easy to error out on create graphics pipeline, so we handle it a bit better than the
    // common VK_CHECK case
    VkPipeline new_pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &new_pipeline) !=
        VK_SUCCESS)
    {
        return VK_NULL_HANDLE;  // failed to create graphics pipeline
    }
    else
    {
        return new_pipeline;
    }
}
}  // namespace render
}  // namespace agea

#pragma once

#include "vulkan_render_types/vulkan_types.h"

namespace agea
{
namespace vk_init
{
VkCommandPoolCreateInfo
command_pool_create_info(uint32_t queue_family_index, VkCommandPoolResetFlags flags = 0);

VkCommandBufferAllocateInfo
command_buffer_allocate_info(VkCommandPool pool,
                             uint32_t count = 1,
                             VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkCommandBufferBeginInfo
command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

VkFramebufferCreateInfo
framebuffer_create_info(VkRenderPass render_pass, VkExtent2D extent);

VkFenceCreateInfo
fence_create_info(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo
semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo
submit_info(VkCommandBuffer* cmd);

VkPresentInfoKHR
present_info();

VkRenderPassBeginInfo
renderpass_begin_info(VkRenderPass render_pass, VkExtent2D extent, VkFramebuffer framebuffer);

VkPipelineShaderStageCreateInfo
pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shader_module);

VkPipelineVertexInputStateCreateInfo
vertex_input_state_create_info();

VkPipelineInputAssemblyStateCreateInfo
input_assembly_create_info(VkPrimitiveTopology topology);

VkPipelineRasterizationStateCreateInfo
rasterization_state_create_info(VkPolygonMode polygon_mode);

VkPipelineMultisampleStateCreateInfo
multisampling_state_create_info();

VkPipelineColorBlendAttachmentState
color_blend_attachment_state();

VkPipelineLayoutCreateInfo
pipeline_layout_create_info();

VkImageCreateInfo
image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);

VkImageViewCreateInfo
imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);

VkPipelineDepthStencilStateCreateInfo
depth_stencil_create_info(bool depth_test, bool depth_trite, VkCompareOp compare_op);

VkDescriptorSetLayoutBinding
descriptorset_layout_binding(VkDescriptorType type,
                             VkShaderStageFlags stage_flags,
                             uint32_t binding);

VkWriteDescriptorSet
write_descriptor_buffer(VkDescriptorType type,
                        VkDescriptorSet dst_set,
                        VkDescriptorBufferInfo* buffer_info,
                        uint32_t binding);

VkWriteDescriptorSet
write_descriptor_image(VkDescriptorType type,
                       VkDescriptorSet dst_set,
                       VkDescriptorImageInfo* image_info,
                       uint32_t binding);

VkSamplerCreateInfo
sampler_create_info(VkFilter filters,
                    VkSamplerAddressMode sampler_adress_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
}  // namespace vk_init
}  // namespace agea
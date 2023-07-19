#pragma once

#include <vulkan/vulkan.h>

#include <vulkan_render/types/vulkan_shader_effects_presets.h>

#include <vector>

namespace agea
{
namespace render
{
namespace vk_utils
{

VkCommandPoolCreateInfo
make_command_pool_create_info(uint32_t queue_family_index, VkCommandPoolResetFlags flags = 0);

VkCommandBufferAllocateInfo
make_command_buffer_allocate_info(VkCommandPool pool,
                                  uint32_t count = 1,
                                  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkCommandBufferBeginInfo
make_command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

VkFramebufferCreateInfo
make_framebuffer_create_info(VkRenderPass render_pass, VkExtent2D extent);

VkFenceCreateInfo
make_fence_create_info(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo
make_semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo
make_submit_info(VkCommandBuffer* cmd);

VkPresentInfoKHR
make_present_info();

VkRenderPassBeginInfo
make_renderpass_begin_info(VkRenderPass render_pass, VkExtent2D extent, VkFramebuffer framebuffer);

VkPipelineShaderStageCreateInfo
make_pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shader_module);

VkPipelineVertexInputStateCreateInfo
make_vertex_input_state_create_info();

VkPipelineInputAssemblyStateCreateInfo
make_input_assembly_create_info(VkPrimitiveTopology topology);

VkPipelineRasterizationStateCreateInfo
make_rasterization_state_create_info(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode);

VkPipelineMultisampleStateCreateInfo
make_multisampling_state_create_info();

VkPipelineColorBlendAttachmentState
make_color_blend_attachment_state(bool enable_alpha);

VkPipelineLayoutCreateInfo
make_pipeline_layout_create_info();

VkImageCreateInfo
make_image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);

VkImageViewCreateInfo
make_imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);

VkPipelineDepthStencilStateCreateInfo
make_depth_stencil_create_info(bool depth_test,
                               bool depth_write,
                               VkCompareOp depth_compare_op,
                               depth_stencil_mode enable_stencil);

VkDescriptorSetLayoutBinding
make_descriptor_set_layout_binding(VkDescriptorType type,
                                   VkShaderStageFlags stage_flags,
                                   uint32_t binding);

VkWriteDescriptorSet
make_write_descriptor_buffer(VkDescriptorType type,
                             VkDescriptorSet dst_set,
                             VkDescriptorBufferInfo* buffer_info,
                             uint32_t binding);

VkWriteDescriptorSet
make_write_descriptor_image(VkDescriptorType type,
                            VkDescriptorSet dst_set,
                            VkDescriptorImageInfo* image_info,
                            uint32_t binding);

VkSamplerCreateInfo
make_sampler_create_info(VkFilter filters,
                         VkSamplerAddressMode sampler_adress_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

VkPipelineDynamicStateCreateInfo
make_pipeline_dynamic_state_create_info(const std::vector<VkDynamicState>& dynamic_states,
                                        VkPipelineDynamicStateCreateFlags flags = 0);
}  // namespace vk_utils
}  // namespace render
}  // namespace agea
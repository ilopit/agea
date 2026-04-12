#pragma once

#include <utils/id.h>
#include <vulkan/vulkan.h>

#include <functional>
#include <vector>

namespace kryga::render
{

// ============================================================================
// Core enums and types used by both render_graph and render_pass
// ============================================================================

enum class rg_access_mode
{
    read = 0,
    write,
    read_write
};

enum class rg_pass_type
{
    graphics = 0,
    compute,
    transfer
};

enum class rg_resource_type
{
    buffer = 0,
    image
};

// Resource descriptor
struct resource_description
{
    utils::id name;
    rg_resource_type type = rg_resource_type::image;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t format = 0;
    bool is_imported = false;
};

// Pass resource reference
struct rg_resource_ref
{
    resource_description* resource = nullptr;
    rg_access_mode usage;
};

// Access info for barrier calculation
struct rg_access_info
{
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags access = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

// Pre-computed barriers for a pass
struct rg_pass_barriers
{
    bool
    empty() const
    {
        return memory_barriers.empty() && buffer_barriers.empty() && image_barriers.empty();
    }

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    std::vector<VkMemoryBarrier> memory_barriers;
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    std::vector<VkImageMemoryBarrier> image_barriers;
};

// ============================================================================
// Subpass types for mobile GPU optimization (tile-based rendering)
// ============================================================================

// How an attachment is used within a subpass
enum class subpass_attachment_usage
{
    color_output,      // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    depth_output,      // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    depth_read_only,   // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    input_attachment,  // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subpassLoad()
    preserve           // VK_ATTACHMENT_UNUSED in subpass, preserved for later
};

// Memory scope for attachment - explicit transient declaration
enum class attachment_memory_scope
{
    persistent,  // Normal DRAM-backed image
    transient    // VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, tile memory only
};

// Describes one attachment across all subpasses
struct subpass_attachment_desc
{
    utils::id name;                 // Resource name in render graph
    VkFormat format;                // Explicit format
    attachment_memory_scope scope;  // transient or persistent
    VkAttachmentLoadOp load_op;     // CLEAR, LOAD, DONT_CARE
    VkAttachmentStoreOp store_op;   // STORE, DONT_CARE
};

// Per-subpass attachment reference
struct subpass_attachment_ref
{
    uint32_t attachment_index;  // Index into subpass_group_desc::attachments
    subpass_attachment_usage usage;
};

// Describes a single subpass within a group
struct subpass_desc
{
    utils::id name;
    std::vector<subpass_attachment_ref> attachments;
    std::function<void(VkCommandBuffer, uint32_t subpass_index)> execute;
};

// Describes a complete multi-subpass render pass
struct subpass_group_desc
{
    utils::id name;
    std::vector<subpass_attachment_desc> attachments;
    std::vector<subpass_desc> subpasses;
    uint32_t width = 0;  // 0 = use frame context
    uint32_t height = 0;
};

}  // namespace kryga::render

#pragma once

#include <utils/id.h>
#include <vulkan/vulkan.h>

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

}  // namespace kryga::render

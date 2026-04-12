#pragma once

#include "vulkan_render/render_graph_types.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"

#include <utils/id.h>
#include <utils/check.h>

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace kryga::render
{

// Forward declarations
class render_pass;
using render_pass_sptr = std::shared_ptr<render_pass>;

// ============================================================================
// Vulkan-specific resource types
// ============================================================================

// Resource with Vulkan state tracking
struct vulkan_resource
{
    resource_description base;
    VkBufferUsageFlags buffer_usage = 0;
    VkImageUsageFlags image_usage = 0;
    VkFormat image_format = VK_FORMAT_UNDEFINED;
    rg_access_info last_access;
    std::variant<vk_utils::vulkan_buffer*, vk_utils::vulkan_image*> binding;
};

// Per-frame render context
struct rg_frame_context
{
    uint32_t swapchain_image_index = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

// Compiled subpass group (runtime data)
struct compiled_subpass_group
{
    subpass_group_desc desc;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;  // Per swapchain image
    std::vector<vk_utils::vulkan_image_sptr> owned_images;
    std::vector<vk_utils::vulkan_image_view_sptr> owned_views;
    std::vector<VkClearValue> clear_values;
    uint32_t width = 0;
    uint32_t height = 0;

    // External image views indexed by [attachment_index][swapchain_index]
    // For single-buffered attachments, swapchain_index is always 0
    std::vector<std::vector<VkImageView>> attachment_views;
};

class vulkan_render_graph
{
public:
    vulkan_render_graph() = default;
    ~vulkan_render_graph() = default;

    // Resource registration
    void
    register_buffer(const utils::id& name, VkBufferUsageFlags usage);

    void
    register_image(const utils::id& name,
                   uint32_t width,
                   uint32_t height,
                   VkFormat format,
                   VkImageUsageFlags usage);

    void
    import_resource(const utils::id& name, rg_resource_type type = rg_resource_type::image);

    // Register a transient image (tile-memory only, no DRAM backing on mobile)
    void
    register_transient_image(const utils::id& name, VkFormat format, VkImageUsageFlags usage);

    // Helper to create resource refs (looks up resource by name)
    rg_resource_ref
    read(const utils::id& name)
    {
        auto it = m_resources.find(name);
        return {it != m_resources.end() ? &it->second.base : nullptr, rg_access_mode::read};
    }

    rg_resource_ref
    write(const utils::id& name)
    {
        auto it = m_resources.find(name);
        return {it != m_resources.end() ? &it->second.base : nullptr, rg_access_mode::write};
    }

    rg_resource_ref
    read_write(const utils::id& name)
    {
        auto it = m_resources.find(name);
        return {it != m_resources.end() ? &it->second.base : nullptr, rg_access_mode::read_write};
    }

    // Pass registration
    void
    add_pass(render_pass_sptr pass);

    render_pass_sptr
    add_compute_pass(const utils::id& name,
                     std::vector<rg_resource_ref> resources,
                     std::function<void(VkCommandBuffer)> execute);

    void
    add_graphics_pass(const utils::id& name,
                      std::vector<rg_resource_ref> resources,
                      render_pass* rp,
                      VkClearColorValue clear_color,
                      std::function<void(VkCommandBuffer)> execute);

    render_pass_sptr
    add_transfer_pass(const utils::id& name,
                      std::vector<rg_resource_ref> resources,
                      std::function<void(VkCommandBuffer)> execute);

    // Subpass groups for mobile GPU optimization
    void
    add_subpass_group(subpass_group_desc desc);

    // Declare that 'consumer' reads 'producer' as an input attachment (tile-local read)
    void
    input_attachment(const utils::id& producer, const utils::id& consumer);

    // Bind image views to a compiled subpass group attachment
    // views[i] is the view to use when swapchain_index == i
    // For single-buffered attachments, pass a single-element vector
    void
    bind_subpass_attachment(const utils::id& group_name,
                            uint32_t attachment_index,
                            std::vector<VkImageView> views);

    // Build framebuffers for a subpass group (call after all attachments bound)
    bool
    finalize_subpass_group(const utils::id& group_name, uint32_t width, uint32_t height);

    // Per-frame resource binding
    void
    begin_frame();

    void
    bind_buffer(const utils::id& name, vk_utils::vulkan_buffer& buf);

    void
    bind_image(const utils::id& name, vk_utils::vulkan_image& img, VkImageLayout initial_layout);

    void
    set_frame_context(uint32_t swapchain_image_index, uint32_t width, uint32_t height);

    // Set the layout an image must be in after all passes complete (e.g. PRESENT_SRC_KHR).
    // The render graph inserts a final barrier after the last pass that writes this resource.
    void
    set_final_layout(const utils::id& name, VkImageLayout layout);

    // Compile and execute
    bool
    compile();

    bool
    execute(VkCommandBuffer cmd, uint32_t swapchain_image_index, uint32_t width, uint32_t height);

    void
    reset();

    // Accessors
    bool
    is_compiled() const
    {
        return m_compiled;
    }

    const std::vector<size_t>&
    get_execution_order() const
    {
        return m_execution_order;
    }

    size_t
    get_pass_count() const
    {
        return m_passes.size();
    }

    render_pass*
    get_pass(const utils::id& name) const;

    const std::unordered_map<utils::id, vulkan_resource>&
    get_resources() const
    {
        return m_resources;
    }

private:
    static rg_access_info
    compute_access_for_usage(rg_access_mode usage,
                             rg_pass_type pass_type,
                             rg_resource_type res_type,
                             VkFormat image_format = VK_FORMAT_UNDEFINED);

    static bool
    needs_barrier(const rg_access_info& prev, const rg_access_info& next);

    void
    calculate_barriers();

    void
    insert_barriers(VkCommandBuffer cmd, const rg_pass_barriers& barriers);

    std::unordered_map<utils::id, vulkan_resource> m_resources;
    std::vector<render_pass_sptr> m_passes;
    std::vector<size_t> m_execution_order;
    bool m_compiled = false;

    std::unordered_set<utils::id> m_bound_this_frame;
    std::unordered_map<utils::id, VkImageLayout> m_final_layouts;

    // Subpass support
    std::unordered_set<utils::id> m_transient_resources;
    std::vector<subpass_group_desc> m_subpass_groups;
    std::vector<compiled_subpass_group> m_compiled_subpass_groups;
    std::unordered_map<utils::id, utils::id> m_input_attachment_links;  // producer -> consumer

    // Maps pass name -> subpass group name (for passes subsumed by a group)
    std::unordered_map<utils::id, utils::id> m_pass_to_group;
    // Tracks which groups have been executed this frame
    std::unordered_set<utils::id> m_executed_groups;

    bool
    compile_subpass_group(const subpass_group_desc& desc, compiled_subpass_group& out);

    void
    execute_subpass_group(VkCommandBuffer cmd,
                          compiled_subpass_group& group,
                          uint32_t swapchain_idx,
                          uint32_t width,
                          uint32_t height);

    void
    cleanup_subpass_groups();
};

}  // namespace kryga::render

#pragma once

#include "vulkan_render/render_graph.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/utils/vulkan_buffer.h"

#include <utils/id.h>

#include <vulkan/vulkan.h>

#include <functional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kryga::render
{

// Access info for barrier calculation
struct rg_access_info
{
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags access = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

// Buffer binding for per-frame resources
struct rg_buffer_binding
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize range = VK_WHOLE_SIZE;
};

// Image binding for per-frame resources
struct rg_image_binding
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

// Vulkan-specific resource with state tracking
struct rg_vk_resource
{
    rg_resource base;
    VkBufferUsageFlags buffer_usage = 0;
    VkImageUsageFlags image_usage = 0;
    rg_access_info last_access;
    std::variant<rg_buffer_binding, rg_image_binding> binding;
};

// Per-frame render context
struct rg_frame_context
{
    uint32_t swapchain_image_index = 0;
    uint32_t width = 0;
    uint32_t height = 0;
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

    // Pass registration - passes are now render_pass objects
    void
    add_pass(render_pass_sptr pass);

    // Convenience methods for creating and adding passes
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

    // Per-frame resource binding
    void
    bind_buffer(const utils::id& name,
                VkBuffer buf,
                VkDeviceSize offset = 0,
                VkDeviceSize range = VK_WHOLE_SIZE);

    void
    bind_image(const utils::id& name,
               VkImage img,
               VkImageView view,
               VkFormat format,
               VkImageLayout layout);

    // Set per-frame context before execute
    void
    set_frame_context(uint32_t swapchain_image_index, uint32_t width, uint32_t height);

    // Compile: topological sort + barrier calculation
    bool
    compile();

    // Execute all passes in dependency order with automatic barriers
    void
    execute(VkCommandBuffer cmd);

    // Reset for reuse
    void
    reset();

    // Accessors
    bool
    is_compiled() const
    {
        return m_compiled;
    }

    const std::string&
    get_error() const
    {
        return m_error;
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

private:
    // Barrier calculation helpers
    static rg_access_info
    compute_access_for_usage(rg_resource_usage usage, rg_pass_type pass_type, rg_resource_type res_type);

    static bool
    needs_barrier(const rg_access_info& prev, const rg_access_info& next);

    void
    calculate_barriers();

    void
    insert_barriers(VkCommandBuffer cmd, const rg_pass_barriers& barriers);

    std::unordered_map<utils::id, rg_vk_resource> m_resources;
    std::vector<render_pass_sptr> m_passes;
    std::vector<size_t> m_execution_order;
    bool m_compiled = false;
    std::string m_error;

    // Per-frame context
    rg_frame_context m_frame_ctx;
};

}  // namespace kryga::render

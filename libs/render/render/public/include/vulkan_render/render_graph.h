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
struct rg_vk_resource
{
    rg_resource base;
    VkBufferUsageFlags buffer_usage = 0;
    VkImageUsageFlags image_usage = 0;
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

// ============================================================================
// Render graph
// ============================================================================

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

    // Per-frame resource binding
    void
    begin_frame();

    void
    bind_buffer(const utils::id& name, vk_utils::vulkan_buffer& buf);

    void
    bind_image(const utils::id& name, vk_utils::vulkan_image& img);

    void
    set_frame_context(uint32_t swapchain_image_index, uint32_t width, uint32_t height);

    // Compile and execute
    bool
    compile();

    bool
    execute(VkCommandBuffer cmd);

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

private:
    static rg_access_info
    compute_access_for_usage(rg_access_mode usage,
                             rg_pass_type pass_type,
                             rg_resource_type res_type);

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

    rg_frame_context m_frame_ctx;
    std::unordered_set<utils::id> m_bound_this_frame;
};

}  // namespace kryga::render

#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/types/vulkan_render_pass_builder.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>
#include <utils/check.h>
#include <utils/kryga_log.h>

#include <algorithm>
#include <stdexcept>

namespace kryga::render
{

void
vulkan_render_graph::register_buffer(const utils::id& name, VkBufferUsageFlags usage)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    auto& res = m_resources[name];
    res.base.name = name;
    res.base.type = rg_resource_type::buffer;
    res.base.is_imported = false;
    res.buffer_usage = usage;
}

void
vulkan_render_graph::register_image(const utils::id& name,
                                    uint32_t width,
                                    uint32_t height,
                                    VkFormat format,
                                    VkImageUsageFlags usage)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    auto& res = m_resources[name];
    res.base.name = name;
    res.base.type = rg_resource_type::image;
    res.base.width = width;
    res.base.height = height;
    res.base.format = static_cast<uint32_t>(format);
    res.base.is_imported = false;
    res.image_usage = usage;
}

void
vulkan_render_graph::import_resource(const utils::id& name, rg_resource_type type)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    auto& res = m_resources[name];
    res.base.name = name;
    res.base.type = type;
    res.base.is_imported = true;
}

void
vulkan_render_graph::register_transient_image(const utils::id& name,
                                              VkFormat format,
                                              VkImageUsageFlags usage)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    // Add TRANSIENT_ATTACHMENT flag for tile-memory-only allocation
    usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    // Size 0 means use frame context dimensions
    register_image(name, 0, 0, format, usage);
    m_transient_resources.insert(name);
}

void
vulkan_render_graph::add_pass(render_pass_sptr pass)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");
    m_passes.push_back(std::move(pass));
}

render_pass_sptr
vulkan_render_graph::add_compute_pass(const utils::id& name,
                                      std::vector<rg_resource_ref> resources,
                                      std::function<void(VkCommandBuffer)> execute)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    auto pass = std::make_shared<render_pass>(name, rg_pass_type::compute);
    pass->resources() = std::move(resources);
    pass->set_execute_callback(std::move(execute));
    m_passes.push_back(pass);
    return pass;
}

void
vulkan_render_graph::add_graphics_pass(const utils::id& name,
                                       std::vector<rg_resource_ref> resources,
                                       render_pass* rp,
                                       VkClearColorValue clear_color,
                                       std::function<void(VkCommandBuffer)> execute)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");
    KRG_check(rp != nullptr, "Graphics pass requires a valid render_pass");

    // Configure the existing render_pass for graph usage
    rp->set_name(name);
    rp->resources() = std::move(resources);
    rp->set_clear_color(clear_color);
    rp->set_execute_callback(std::move(execute));

    // Store as shared_ptr with no-op deleter since we don't own the render_pass
    m_passes.push_back(render_pass_sptr(rp, [](render_pass*) {}));
}

render_pass_sptr
vulkan_render_graph::add_transfer_pass(const utils::id& name,
                                       std::vector<rg_resource_ref> resources,
                                       std::function<void(VkCommandBuffer)> execute)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    auto pass = std::make_shared<render_pass>(name, rg_pass_type::transfer);
    pass->resources() = std::move(resources);
    pass->set_execute_callback(std::move(execute));
    m_passes.push_back(pass);
    return pass;
}

void
vulkan_render_graph::add_subpass_group(subpass_group_desc desc)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");
    KRG_check(!desc.subpasses.empty(), "Subpass group must have at least one subpass");

    // Map each subpass name to the group name
    for (const auto& sp : desc.subpasses)
    {
        m_pass_to_group[sp.name] = desc.name;
    }

    m_subpass_groups.push_back(std::move(desc));
}

void
vulkan_render_graph::input_attachment(const utils::id& producer, const utils::id& consumer)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    m_input_attachment_links[producer] = consumer;
}

void
vulkan_render_graph::bind_subpass_attachment(const utils::id& group_name,
                                             uint32_t attachment_index,
                                             std::vector<VkImageView> views)
{
    KRG_check(m_compiled, "Graph must be compiled first");

    for (auto& group : m_compiled_subpass_groups)
    {
        if (group.desc.name == group_name)
        {
            if (attachment_index >= group.attachment_views.size())
            {
                group.attachment_views.resize(attachment_index + 1);
            }
            group.attachment_views[attachment_index] = std::move(views);
            return;
        }
    }
    KRG_check(false, "Subpass group not found");
}

bool
vulkan_render_graph::finalize_subpass_group(const utils::id& group_name,
                                            uint32_t width,
                                            uint32_t height)
{
    KRG_check(m_compiled, "Graph must be compiled first");

    auto& device = glob::glob_state().getr_render_device();
    auto vk_device = device.vk_device();

    for (auto& group : m_compiled_subpass_groups)
    {
        if (group.desc.name != group_name)
        {
            continue;
        }

        group.width = width;
        group.height = height;

        // Destroy old framebuffers if any
        for (auto fb : group.framebuffers)
        {
            if (fb != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(vk_device, fb, nullptr);
            }
        }
        group.framebuffers.clear();

        // Determine number of framebuffers needed (max swapchain count across attachments)
        size_t fb_count = 1;
        for (const auto& views : group.attachment_views)
        {
            fb_count = std::max(fb_count, views.size());
        }

        // Create framebuffers
        for (size_t fb_idx = 0; fb_idx < fb_count; ++fb_idx)
        {
            std::vector<VkImageView> fb_views;
            fb_views.reserve(group.attachment_views.size());

            for (const auto& views : group.attachment_views)
            {
                // Use modulo for single-buffered attachments
                size_t view_idx = fb_idx % views.size();
                fb_views.push_back(views[view_idx]);
            }

            VkFramebufferCreateInfo fb_ci = {};
            fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_ci.renderPass = group.render_pass;
            fb_ci.attachmentCount = static_cast<uint32_t>(fb_views.size());
            fb_ci.pAttachments = fb_views.data();
            fb_ci.width = width;
            fb_ci.height = height;
            fb_ci.layers = 1;

            VkFramebuffer fb = VK_NULL_HANDLE;
            VkResult result = vkCreateFramebuffer(vk_device, &fb_ci, nullptr, &fb);
            if (result != VK_SUCCESS)
            {
                ALOG_ERROR("Failed to create subpass group framebuffer");
                return false;
            }
            group.framebuffers.push_back(fb);
        }

        ALOG_INFO("Finalized subpass group '{}': {} framebuffers, {}x{}",
                  group_name.str(),
                  group.framebuffers.size(),
                  width,
                  height);
        return true;
    }

    ALOG_ERROR("Subpass group '{}' not found", group_name.str());
    return false;
}

void
vulkan_render_graph::begin_frame()
{
    m_bound_this_frame.clear();
    m_executed_groups.clear();
}

void
vulkan_render_graph::bind_buffer(const utils::id& name, vk_utils::vulkan_buffer& buf)
{
    auto it = m_resources.find(name);
    KRG_check(it != m_resources.end(), "Resource not registered");
    KRG_check(buf.buffer() != VK_NULL_HANDLE, "Cannot bind null buffer");

    it->second.binding = &buf;
    m_bound_this_frame.insert(name);
}

void
vulkan_render_graph::bind_image(const utils::id& name,
                                vk_utils::vulkan_image& img,
                                VkImageLayout initial_layout)
{
    auto it = m_resources.find(name);
    KRG_check(it != m_resources.end(), "Resource not registered");
    KRG_check(img.image() != VK_NULL_HANDLE, "Cannot bind null image");

    it->second.binding = &img;
    it->second.image_format = img.format();
    it->second.last_access.layout = initial_layout;
    m_bound_this_frame.insert(name);
}

void
vulkan_render_graph::set_frame_context(uint32_t swapchain_image_index,
                                       uint32_t width,
                                       uint32_t height)
{
}

void
vulkan_render_graph::set_final_layout(const utils::id& name, VkImageLayout layout)
{
    m_final_layouts[name] = layout;
}

bool
vulkan_render_graph::compile()
{
    if (m_compiled)
    {
        return true;
    }

    // Validate all resource refs exist
    for (const auto& pass : m_passes)
    {
        for (const auto& ref : pass->resources())
        {
            if (!ref.resource || m_resources.find(ref.resource->name) == m_resources.end())
            {
                ALOG_ERROR("Pass {} doesn't have {}",
                           pass->name().str(),
                           ref.resource ? ref.resource->name.str() : "null");

                return false;
            }
        }
    }

    // Build dependency graph using writer/reader tracking
    std::unordered_map<utils::id, std::vector<size_t>> writers;
    std::unordered_map<utils::id, std::vector<size_t>> readers;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        for (const auto& ref : m_passes[i]->resources())
        {
            if (!ref.resource)
            {
                continue;
            }

            if (ref.usage == rg_access_mode::write || ref.usage == rg_access_mode::read_write)
            {
                writers[ref.resource->name].push_back(i);
            }
            if (ref.usage == rg_access_mode::read || ref.usage == rg_access_mode::read_write)
            {
                readers[ref.resource->name].push_back(i);
            }
        }
    }

    // Build adjacency list (pass depends on writers of resources it reads)
    std::vector<std::vector<size_t>> deps(m_passes.size());
    for (const auto& [resource, reader_indices] : readers)
    {
        auto it = writers.find(resource);
        if (it != writers.end())
        {
            for (size_t reader_idx : reader_indices)
            {
                for (size_t writer_idx : it->second)
                {
                    if (reader_idx != writer_idx)
                    {
                        deps[reader_idx].push_back(writer_idx);
                    }
                }
            }
        }
    }

    // Topological sort (Kahn's algorithm)
    std::vector<std::vector<size_t>> dependents(m_passes.size());
    std::vector<size_t> in_degree(m_passes.size(), 0);

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        for (size_t dep : deps[i])
        {
            dependents[dep].push_back(i);
        }
        in_degree[i] = deps[i].size();
    }

    std::vector<size_t> queue;
    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        if (in_degree[i] == 0)
        {
            queue.push_back(i);
        }
    }

    m_execution_order.clear();
    while (!queue.empty())
    {
        size_t curr = queue.back();
        queue.pop_back();
        m_execution_order.push_back(curr);

        for (size_t dependent : dependents[curr])
        {
            in_degree[dependent]--;
            if (in_degree[dependent] == 0)
            {
                queue.push_back(dependent);
            }
        }
    }

    if (m_execution_order.size() != m_passes.size())
    {
        ALOG_ERROR("Cycle detected in render graph ");
        return false;
    }

    // Assign order
    for (size_t i = 0; i < m_execution_order.size(); ++i)
    {
        m_passes[m_execution_order[i]]->set_order(static_cast<uint32_t>(i));
    }

    // Calculate barriers for each pass
    calculate_barriers();

    // Validate all passes: binding table resources + BDA push constant fields
    for (const auto& pass : m_passes)
    {
        if (!pass->validate_resources(*this))
        {
            return false;
        }
    }

    // Compile subpass groups
    m_compiled_subpass_groups.clear();
    for (const auto& group_desc : m_subpass_groups)
    {
        compiled_subpass_group compiled;
        if (!compile_subpass_group(group_desc, compiled))
        {
            ALOG_ERROR("Failed to compile subpass group: {}", group_desc.name.str());
            return false;
        }
        m_compiled_subpass_groups.push_back(std::move(compiled));
    }

    m_compiled = true;
    return true;
}

bool
vulkan_render_graph::execute(VkCommandBuffer cmd,
                             uint32_t swapchain_image_index,
                             uint32_t width,
                             uint32_t height)
{
    if (!m_compiled)
    {
        if (!compile())
        {
            return false;
        }
    }

    // Validate all registered resources are bound this frame
    for (const auto& [name, res] : m_resources)
    {
        if (m_bound_this_frame.find(name) == m_bound_this_frame.end())
        {
            ALOG_ERROR("Resource not bound this frame");
            return false;
        }
    }

    // Reset stage/access state for this frame, but preserve image layouts
    // Images retain their layout from the previous frame (e.g. shadow maps stay in
    // SHADER_READ_ONLY_OPTIMAL after main pass). The graph will insert barriers to
    // transition them to the required layout for the first pass that uses them.
    for (auto& [name, res] : m_resources)
    {
        VkImageLayout prev_layout = res.last_access.layout;
        res.last_access = rg_access_info{};
        if (res.base.type == rg_resource_type::image)
        {
            res.last_access.layout = prev_layout;
        }
    }

    for (size_t idx : m_execution_order)
    {
        auto& pass = m_passes[idx];

        // Check if this pass is part of a subpass group
        auto group_it = m_pass_to_group.find(pass->name());
        if (group_it != m_pass_to_group.end())
        {
            const auto& group_name = group_it->second;

            // Already executed this group?
            if (m_executed_groups.count(group_name) > 0)
            {
                // Skip - group was already executed when we hit its first pass
                continue;
            }

            // Find the compiled group
            compiled_subpass_group* group = nullptr;
            for (auto& g : m_compiled_subpass_groups)
            {
                if (g.desc.name == group_name)
                {
                    group = &g;
                    break;
                }
            }

            if (!group || group->framebuffers.empty())
            {
                ALOG_ERROR("Subpass group '{}' not finalized", group_name.str());
                return false;
            }

            // Insert barriers for resources used by the group
            // (Simplified: just use barriers for this pass's resources)
            rg_pass_barriers barriers;
            for (const auto& ref : pass->resources())
            {
                if (!ref.resource)
                    continue;

                auto it = m_resources.find(ref.resource->name);
                if (it == m_resources.end())
                    continue;

                auto& res = it->second;
                rg_access_info required = compute_access_for_usage(
                    ref.usage, pass->type(), ref.resource->type, res.image_format);

                if (needs_barrier(res.last_access, required))
                {
                    barriers.src_stage |= res.last_access.stage;
                    barriers.dst_stage |= required.stage;

                    if (ref.resource->type == rg_resource_type::image)
                    {
                        if (auto** img = std::get_if<vk_utils::vulkan_image*>(&res.binding))
                        {
                            bool is_depth = res.image_format == VK_FORMAT_D32_SFLOAT ||
                                            res.image_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                            res.image_format == VK_FORMAT_D16_UNORM ||
                                            res.image_format == VK_FORMAT_D24_UNORM_S8_UINT;

                            VkImageMemoryBarrier barrier = {};
                            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                            barrier.srcAccessMask = res.last_access.access;
                            barrier.dstAccessMask = required.access;
                            barrier.oldLayout = res.last_access.layout;
                            barrier.newLayout = required.layout;
                            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.image = (*img)->image();
                            barrier.subresourceRange.aspectMask =
                                is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                            barrier.subresourceRange.baseMipLevel = 0;
                            barrier.subresourceRange.levelCount = 1;
                            barrier.subresourceRange.baseArrayLayer = 0;
                            barrier.subresourceRange.layerCount = 1;
                            barriers.image_barriers.push_back(barrier);
                        }
                    }
                }
                res.last_access = required;
            }

            insert_barriers(cmd, barriers);

            // Execute the subpass group
            execute_subpass_group(cmd, *group, swapchain_image_index, width, height);
            m_executed_groups.insert(group_name);
            continue;
        }

        // Regular pass (not part of a subpass group)
        rg_pass_barriers barriers;
        for (const auto& ref : pass->resources())
        {
            if (!ref.resource)
            {
                continue;
            }

            auto it = m_resources.find(ref.resource->name);
            if (it == m_resources.end())
            {
                continue;
            }

            auto& res = it->second;
            rg_access_info required = compute_access_for_usage(
                ref.usage, pass->type(), ref.resource->type, res.image_format);

            if (needs_barrier(res.last_access, required))
            {
                barriers.src_stage |= res.last_access.stage;
                barriers.dst_stage |= required.stage;

                if (ref.resource->type == rg_resource_type::buffer)
                {
                    if (auto** buf = std::get_if<vk_utils::vulkan_buffer*>(&res.binding))
                    {
                        VkBufferMemoryBarrier barrier = {};
                        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                        barrier.srcAccessMask = res.last_access.access;
                        barrier.dstAccessMask = required.access;
                        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.buffer = (*buf)->buffer();
                        barrier.offset = 0;
                        barrier.size = (*buf)->get_alloc_size();
                        barriers.buffer_barriers.push_back(barrier);
                    }
                }
                else if (ref.resource->type == rg_resource_type::image)
                {
                    if (auto** img = std::get_if<vk_utils::vulkan_image*>(&res.binding))
                    {
                        bool is_depth = res.image_format == VK_FORMAT_D32_SFLOAT ||
                                        res.image_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                        res.image_format == VK_FORMAT_D16_UNORM ||
                                        res.image_format == VK_FORMAT_D24_UNORM_S8_UINT;

                        VkImageMemoryBarrier barrier = {};
                        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        barrier.srcAccessMask = res.last_access.access;
                        barrier.dstAccessMask = required.access;
                        barrier.oldLayout = res.last_access.layout;
                        barrier.newLayout = required.layout;
                        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.image = (*img)->image();
                        barrier.subresourceRange.aspectMask =
                            is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                        barrier.subresourceRange.baseMipLevel = 0;
                        barrier.subresourceRange.levelCount = 1;
                        barrier.subresourceRange.baseArrayLayer = 0;
                        barrier.subresourceRange.layerCount = 1;
                        barriers.image_barriers.push_back(barrier);
                    }
                }
            }

            // Update resource state
            res.last_access = required;
        }

        // Insert barriers if needed
        insert_barriers(cmd, barriers);

        // Execute pass
        pass->execute(cmd, swapchain_image_index, width, height);
    }

    // Final layout transitions (e.g. COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR)
    for (const auto& [name, final_layout] : m_final_layouts)
    {
        auto it = m_resources.find(name);
        if (it == m_resources.end())
        {
            continue;
        }

        auto& res = it->second;
        if (res.last_access.layout == final_layout)
        {
            continue;
        }

        if (auto** img = std::get_if<vk_utils::vulkan_image*>(&res.binding))
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = res.last_access.access;
            barrier.dstAccessMask = 0;
            barrier.oldLayout = res.last_access.layout;
            barrier.newLayout = final_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = (*img)->image();
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            vkCmdPipelineBarrier(cmd,
                                 res.last_access.stage,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);

            res.last_access.layout = final_layout;
        }
    }

    return true;
}

void
vulkan_render_graph::reset()
{
    cleanup_subpass_groups();
    m_resources.clear();
    m_passes.clear();
    m_execution_order.clear();
    m_final_layouts.clear();
    m_transient_resources.clear();
    m_subpass_groups.clear();
    m_compiled_subpass_groups.clear();
    m_input_attachment_links.clear();
    m_pass_to_group.clear();
    m_executed_groups.clear();
    m_compiled = false;
}

render_pass*
vulkan_render_graph::get_pass(const utils::id& name) const
{
    for (const auto& pass : m_passes)
    {
        if (pass->name() == name)
        {
            return pass.get();
        }
    }
    return nullptr;
}

static bool
is_depth_format(VkFormat fmt)
{
    return fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D24_UNORM_S8_UINT;
}

rg_access_info
vulkan_render_graph::compute_access_for_usage(rg_access_mode usage,
                                              rg_pass_type pass_type,
                                              rg_resource_type res_type,
                                              VkFormat image_format)
{
    rg_access_info info;

    bool depth = is_depth_format(image_format);

    // Determine pipeline stage based on pass type
    switch (pass_type)
    {
    case rg_pass_type::compute:
    {
        info.stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        break;
    }
    case rg_pass_type::transfer:
    {
        info.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    }
    case rg_pass_type::graphics:
    default:
    {
        if (res_type == rg_resource_type::image)
        {
            if (usage == rg_access_mode::read)
            {
                info.stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (depth)
            {
                info.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }
            else
            {
                info.stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
        }
        else
        {
            info.stage =
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        break;
    }
    }

    // Determine access flags and layout based on usage
    switch (usage)
    {
    case rg_access_mode::read:
    {
        if (pass_type == rg_pass_type::transfer)
        {
            info.access = VK_ACCESS_TRANSFER_READ_BIT;
            info.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
        else
        {
            info.access = VK_ACCESS_SHADER_READ_BIT;
            info.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        break;
    }
    case rg_access_mode::write:
    {
        if (pass_type == rg_pass_type::transfer)
        {
            info.access = VK_ACCESS_TRANSFER_WRITE_BIT;
            info.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        else if (pass_type == rg_pass_type::graphics && res_type == rg_resource_type::image)
        {
            if (depth)
            {
                info.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                info.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            else
            {
                info.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                info.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
        }
        else
        {
            info.access = VK_ACCESS_SHADER_WRITE_BIT;
            info.layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        break;
    }

    case rg_access_mode::read_write:
    {
        info.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        info.layout = VK_IMAGE_LAYOUT_GENERAL;
        break;
    }
    }

    return info;
}

bool
vulkan_render_graph::needs_barrier(const rg_access_info& prev, const rg_access_info& next)
{
    // Need barrier if:
    // 1. Previous access was a write (WAR, WAW hazards)
    // 2. Layout transition is needed for images
    // 3. Stage transition crosses pipeline boundaries

    bool prev_wrote =
        (prev.access &
         (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0;

    bool layout_change = (prev.layout != next.layout);

    // For RAW hazards: need barrier if previous wrote and next reads
    bool raw_hazard =
        prev_wrote && (next.access & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
                                      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)) != 0;

    // For WAW hazards: need barrier if both write
    bool next_writes =
        (next.access & (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_TRANSFER_WRITE_BIT)) != 0;
    bool waw_hazard = prev_wrote && next_writes;

    return layout_change || raw_hazard || waw_hazard;
}

void
vulkan_render_graph::calculate_barriers()
{
    // Pre-calculate barriers for static analysis (optional optimization)
    // For now, barriers are calculated dynamically during execute()
}

void
vulkan_render_graph::insert_barriers(VkCommandBuffer cmd, const rg_pass_barriers& barriers)
{
    if (barriers.empty())
    {
        return;
    }

    VkPipelineStageFlags src_stage = barriers.src_stage;
    VkPipelineStageFlags dst_stage = barriers.dst_stage;

    // Ensure valid stage masks
    if (src_stage == 0)
    {
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    if (dst_stage == 0)
    {
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd,
                         src_stage,
                         dst_stage,
                         0,
                         static_cast<uint32_t>(barriers.memory_barriers.size()),
                         barriers.memory_barriers.data(),
                         static_cast<uint32_t>(barriers.buffer_barriers.size()),
                         barriers.buffer_barriers.data(),
                         static_cast<uint32_t>(barriers.image_barriers.size()),
                         barriers.image_barriers.data());
}

// ============================================================================
// Subpass group support
// ============================================================================

bool
vulkan_render_graph::compile_subpass_group(const subpass_group_desc& desc,
                                           compiled_subpass_group& out)
{
    auto& device = glob::glob_state().getr_render_device();
    auto vk_device = device.vk_device();

    out.desc = desc;

    // Build VkRenderPass
    multi_subpass_render_pass_builder builder(vk_device);
    out.render_pass = builder.build(desc);
    if (out.render_pass == VK_NULL_HANDLE)
    {
        ALOG_ERROR("Failed to build subpass group render pass");
        return false;
    }

    out.clear_values = builder.get_clear_values(desc);

    // Create transient images for attachments that aren't externally bound
    // For now, we expect all attachments to be bound externally via bind_image()
    // Transient allocation will be added when we integrate with resource management

    ALOG_INFO("Compiled subpass group '{}' with {} subpasses",
              desc.name.str(),
              desc.subpasses.size());

    return true;
}

void
vulkan_render_graph::execute_subpass_group(VkCommandBuffer cmd,
                                           compiled_subpass_group& group,
                                           uint32_t swapchain_idx,
                                           uint32_t width,
                                           uint32_t height)
{
    if (group.framebuffers.empty())
    {
        ALOG_ERROR("Subpass group '{}' has no framebuffers - call finalize_subpass_group first",
                   group.desc.name.str());
        return;
    }

    // Select framebuffer for this swapchain index
    size_t fb_idx = swapchain_idx % group.framebuffers.size();
    VkFramebuffer fb = group.framebuffers[fb_idx];

    // Use stored dimensions or passed dimensions
    uint32_t render_width = group.width > 0 ? group.width : width;
    uint32_t render_height = group.height > 0 ? group.height : height;

    // Begin render pass
    VkRenderPassBeginInfo rp_begin = {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = group.render_pass;
    rp_begin.framebuffer = fb;
    rp_begin.renderArea.offset = {0, 0};
    rp_begin.renderArea.extent = {render_width, render_height};
    rp_begin.clearValueCount = static_cast<uint32_t>(group.clear_values.size());
    rp_begin.pClearValues = group.clear_values.data();

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    // Execute each subpass
    for (size_t sp_idx = 0; sp_idx < group.desc.subpasses.size(); ++sp_idx)
    {
        if (sp_idx > 0)
        {
            vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
        }

        const auto& subpass = group.desc.subpasses[sp_idx];
        if (subpass.execute)
        {
            subpass.execute(cmd, static_cast<uint32_t>(sp_idx));
        }
    }

    vkCmdEndRenderPass(cmd);
}

void
vulkan_render_graph::cleanup_subpass_groups()
{
    auto& device = glob::glob_state().getr_render_device();
    auto vk_device = device.vk_device();

    for (auto& group : m_compiled_subpass_groups)
    {
        // Destroy framebuffers
        for (auto fb : group.framebuffers)
        {
            if (fb != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(vk_device, fb, nullptr);
            }
        }
        group.framebuffers.clear();

        // Destroy render pass
        if (group.render_pass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(vk_device, group.render_pass, nullptr);
            group.render_pass = VK_NULL_HANDLE;
        }

        // Owned images/views are cleaned up by shared_ptr destructors
        group.owned_views.clear();
        group.owned_images.clear();
        group.attachment_views.clear();
    }
}

}  // namespace kryga::render

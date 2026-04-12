#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/types/vulkan_render_pass.h"

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
vulkan_render_graph::begin_frame()
{
    m_bound_this_frame.clear();
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

        // Calculate and insert barriers for this pass
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
    m_resources.clear();
    m_passes.clear();
    m_execution_order.clear();
    m_final_layouts.clear();
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

}  // namespace kryga::render

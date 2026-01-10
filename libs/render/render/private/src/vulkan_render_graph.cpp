#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/types/vulkan_render_pass.h"

#include <utils/check.h>

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
vulkan_render_graph::add_compute_pass(const utils::id& name,
                                      std::vector<rg_resource_ref> resources,
                                      std::function<void(VkCommandBuffer)> execute)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    rg_vk_pass pass;
    pass.name = name;
    pass.type = rg_pass_type::compute;
    pass.resources = std::move(resources);
    pass.execute = std::move(execute);
    m_passes.push_back(std::move(pass));
}

void
vulkan_render_graph::add_graphics_pass(const utils::id& name,
                                       std::vector<rg_resource_ref> resources,
                                       render_pass* rp,
                                       VkClearColorValue clear_color,
                                       std::function<void(VkCommandBuffer)> execute)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    rg_vk_pass pass;
    pass.name = name;
    pass.type = rg_pass_type::graphics;
    pass.resources = std::move(resources);
    pass.rp = rp;
    pass.clear_color = clear_color;
    pass.execute = std::move(execute);
    m_passes.push_back(std::move(pass));
}

void
vulkan_render_graph::add_transfer_pass(const utils::id& name,
                                       std::vector<rg_resource_ref> resources,
                                       std::function<void(VkCommandBuffer)> execute)
{
    KRG_check(!m_compiled, "Cannot modify compiled graph");

    rg_vk_pass pass;
    pass.name = name;
    pass.type = rg_pass_type::transfer;
    pass.resources = std::move(resources);
    pass.execute = std::move(execute);
    m_passes.push_back(std::move(pass));
}

void
vulkan_render_graph::bind_buffer(const utils::id& name,
                                 VkBuffer buf,
                                 VkDeviceSize offset,
                                 VkDeviceSize range)
{
    auto it = m_resources.find(name);
    if (it == m_resources.end())
    {
        return;
    }

    it->second.binding = rg_buffer_binding{buf, offset, range};
}

void
vulkan_render_graph::bind_image(const utils::id& name,
                                VkImage img,
                                VkImageView view,
                                VkFormat format,
                                VkImageLayout layout)
{
    auto it = m_resources.find(name);
    if (it == m_resources.end())
    {
        return;
    }

    it->second.binding = rg_image_binding{img, view, format, layout};
    it->second.last_access.layout = layout;
}

void
vulkan_render_graph::set_frame_context(uint32_t swapchain_image_index, uint32_t width, uint32_t height)
{
    m_frame_ctx.swapchain_image_index = swapchain_image_index;
    m_frame_ctx.width = width;
    m_frame_ctx.height = height;
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
        for (const auto& ref : pass.resources)
        {
            if (m_resources.find(ref.resource) == m_resources.end())
            {
                m_error = "Pass '" + std::string(pass.name.cstr()) +
                          "' references unknown resource '" + std::string(ref.resource.cstr()) + "'";
                return false;
            }
        }
    }

    // Build dependency graph using writer/reader tracking
    std::unordered_map<utils::id, std::vector<size_t>> writers;
    std::unordered_map<utils::id, std::vector<size_t>> readers;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        for (const auto& ref : m_passes[i].resources)
        {
            if (ref.usage == rg_resource_usage::write || ref.usage == rg_resource_usage::read_write)
            {
                writers[ref.resource].push_back(i);
            }
            if (ref.usage == rg_resource_usage::read || ref.usage == rg_resource_usage::read_write)
            {
                readers[ref.resource].push_back(i);
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
        m_error = "Cycle detected in render graph";
        return false;
    }

    // Assign order
    for (size_t i = 0; i < m_execution_order.size(); ++i)
    {
        m_passes[m_execution_order[i]].order = static_cast<uint32_t>(i);
    }

    // Calculate barriers for each pass
    calculate_barriers();

    m_compiled = true;
    return true;
}

void
vulkan_render_graph::execute(VkCommandBuffer cmd)
{
    if (!m_compiled)
    {
        if (!compile())
        {
            throw std::runtime_error("Failed to compile render graph: " + m_error);
        }
    }

    // Reset resource access state for this frame
    for (auto& [name, res] : m_resources)
    {
        res.last_access = rg_access_info{};
        // Preserve image layout from binding if available
        if (res.base.type == rg_resource_type::image)
        {
            if (auto* img = std::get_if<rg_image_binding>(&res.binding))
            {
                res.last_access.layout = img->current_layout;
            }
        }
    }

    for (size_t idx : m_execution_order)
    {
        auto& pass = m_passes[idx];

        // Calculate and insert barriers for this pass
        rg_pass_barriers barriers;
        for (const auto& ref : pass.resources)
        {
            auto it = m_resources.find(ref.resource);
            if (it == m_resources.end())
                continue;

            auto& res = it->second;
            rg_access_info required = compute_access_for_usage(ref.usage, pass.type, res.base.type);

            if (needs_barrier(res.last_access, required))
            {
                barriers.src_stage |= res.last_access.stage;
                barriers.dst_stage |= required.stage;

                if (res.base.type == rg_resource_type::buffer)
                {
                    if (auto* buf = std::get_if<rg_buffer_binding>(&res.binding))
                    {
                        VkBufferMemoryBarrier barrier = {};
                        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                        barrier.srcAccessMask = res.last_access.access;
                        barrier.dstAccessMask = required.access;
                        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.buffer = buf->buffer;
                        barrier.offset = buf->offset;
                        barrier.size = buf->range;
                        barriers.buffer_barriers.push_back(barrier);
                    }
                }
                else
                {
                    if (auto* img = std::get_if<rg_image_binding>(&res.binding))
                    {
                        VkImageMemoryBarrier barrier = {};
                        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        barrier.srcAccessMask = res.last_access.access;
                        barrier.dstAccessMask = required.access;
                        barrier.oldLayout = res.last_access.layout;
                        barrier.newLayout = required.layout;
                        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.image = img->image;
                        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
        if (pass.type == rg_pass_type::graphics && pass.rp != nullptr)
        {
            // Use render_pass begin/end
            pass.rp->begin(cmd, m_frame_ctx.swapchain_image_index,
                          m_frame_ctx.width, m_frame_ctx.height, pass.clear_color);

            if (pass.execute)
            {
                pass.execute(cmd);
            }

            pass.rp->end(cmd);
        }
        else
        {
            // Compute or transfer pass (no render pass wrapper)
            if (pass.execute)
            {
                pass.execute(cmd);
            }
        }
    }
}

void
vulkan_render_graph::reset()
{
    m_resources.clear();
    m_passes.clear();
    m_execution_order.clear();
    m_compiled = false;
    m_error.clear();
}

const rg_vk_pass*
vulkan_render_graph::get_pass(const utils::id& name) const
{
    for (const auto& pass : m_passes)
    {
        if (pass.name == name)
        {
            return &pass;
        }
    }
    return nullptr;
}

rg_access_info
vulkan_render_graph::compute_access_for_usage(rg_resource_usage usage,
                                              rg_pass_type pass_type,
                                              rg_resource_type res_type)
{
    rg_access_info info;

    // Determine pipeline stage based on pass type
    switch (pass_type)
    {
    case rg_pass_type::compute:
        info.stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        break;
    case rg_pass_type::transfer:
        info.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case rg_pass_type::graphics:
    default:
        // For graphics, assume fragment shader for reads, color attachment for writes
        if (res_type == rg_resource_type::image)
        {
            info.stage = (usage == rg_resource_usage::read)
                             ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                             : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else
        {
            info.stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        break;
    }

    // Determine access flags based on usage and resource type
    switch (usage)
    {
    case rg_resource_usage::read:
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

    case rg_resource_usage::write:
        if (pass_type == rg_pass_type::transfer)
        {
            info.access = VK_ACCESS_TRANSFER_WRITE_BIT;
            info.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        else if (pass_type == rg_pass_type::graphics && res_type == rg_resource_type::image)
        {
            info.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            info.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        else
        {
            info.access = VK_ACCESS_SHADER_WRITE_BIT;
            info.layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        break;

    case rg_resource_usage::read_write:
        info.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        info.layout = VK_IMAGE_LAYOUT_GENERAL;
        break;
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

    bool prev_wrote = (prev.access & (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0;

    bool layout_change = (prev.layout != next.layout) &&
                         (prev.layout != VK_IMAGE_LAYOUT_UNDEFINED) &&
                         (next.layout != VK_IMAGE_LAYOUT_UNDEFINED);

    // For RAW hazards: need barrier if previous wrote and next reads
    bool raw_hazard = prev_wrote && (next.access & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
                                                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)) != 0;

    // For WAW hazards: need barrier if both write
    bool next_writes = (next.access & (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
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
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dst_stage == 0)
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

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

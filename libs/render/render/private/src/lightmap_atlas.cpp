#include "vulkan_render/lightmap_atlas.h"

#include <algorithm>

namespace kryga
{
namespace render
{

lightmap_atlas::lightmap_atlas(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
{
    m_skyline.push_back({0, 0, width});
}

bool
lightmap_atlas::allocate(const utils::id& mesh_id, uint32_t region_width, uint32_t region_height)
{
    if (region_width == 0 || region_height == 0)
    {
        return false;
    }

    uint32_t out_x = 0, out_y = 0;
    if (!skyline_pack(region_width, region_height, out_x, out_y))
    {
        return false;
    }

    atlas_region region;
    region.x = out_x;
    region.y = out_y;
    region.width = region_width;
    region.height = region_height;

    m_regions[mesh_id] = region;
    return true;
}

const atlas_region*
lightmap_atlas::get_region(const utils::id& mesh_id) const
{
    auto it = m_regions.find(mesh_id);
    return it != m_regions.end() ? &it->second : nullptr;
}

void
lightmap_atlas::remap_uv2(const utils::id& mesh_id, float& u, float& v) const
{
    auto it = m_regions.find(mesh_id);
    if (it == m_regions.end())
    {
        return;
    }

    const auto& r = it->second;
    u = (r.x + u * r.width) / static_cast<float>(m_width);
    v = (r.y + v * r.height) / static_cast<float>(m_height);
}

void
lightmap_atlas::clear()
{
    m_regions.clear();
    m_skyline.clear();
    m_skyline.push_back({0, 0, m_width});
}

bool
lightmap_atlas::skyline_pack(uint32_t rw, uint32_t rh, uint32_t& out_x, uint32_t& out_y)
{
    // Skyline bottom-left algorithm: find the best position along the skyline
    int best_idx = -1;
    uint32_t best_y = UINT32_MAX;
    uint32_t best_waste = UINT32_MAX;

    for (size_t i = 0; i < m_skyline.size(); ++i)
    {
        // Check if rectangle fits starting at skyline node i
        uint32_t x = m_skyline[i].x;
        if (x + rw > m_width)
        {
            continue;
        }

        // Find the maximum y across all skyline nodes this rect would span
        uint32_t max_y = m_skyline[i].y;
        uint32_t remaining_width = rw;
        size_t j = i;

        bool fits = true;
        while (remaining_width > 0 && j < m_skyline.size())
        {
            max_y = std::max(max_y, m_skyline[j].y);
            if (max_y + rh > m_height)
            {
                fits = false;
                break;
            }

            uint32_t consumed = std::min(remaining_width, m_skyline[j].width);
            remaining_width -= consumed;
            ++j;
        }

        if (!fits || remaining_width > 0)
        {
            continue;
        }

        // Compute waste (area below the placed rect above the skyline)
        uint32_t waste = 0;
        {
            uint32_t rem = rw;
            for (size_t k = i; k < m_skyline.size() && rem > 0; ++k)
            {
                uint32_t w = std::min(rem, m_skyline[k].width);
                waste += w * (max_y - m_skyline[k].y);
                rem -= w;
            }
        }

        if (max_y < best_y || (max_y == best_y && waste < best_waste))
        {
            best_y = max_y;
            best_waste = waste;
            best_idx = static_cast<int>(i);
        }
    }

    if (best_idx < 0)
    {
        return false;
    }

    out_x = m_skyline[best_idx].x;
    out_y = best_y;

    // Insert new skyline node for the placed rect
    skyline_node new_node;
    new_node.x = out_x;
    new_node.y = best_y + rh;
    new_node.width = rw;

    // Remove/shrink nodes that the new rect covers
    uint32_t remaining = rw;
    auto it = m_skyline.begin() + best_idx;

    while (remaining > 0 && it != m_skyline.end())
    {
        if (it->width <= remaining)
        {
            remaining -= it->width;
            it = m_skyline.erase(it);
        }
        else
        {
            it->x += remaining;
            it->width -= remaining;
            remaining = 0;
        }
    }

    m_skyline.insert(m_skyline.begin() + best_idx, new_node);

    // Merge adjacent nodes at the same height
    for (size_t k = 0; k + 1 < m_skyline.size();)
    {
        if (m_skyline[k].y == m_skyline[k + 1].y)
        {
            m_skyline[k].width += m_skyline[k + 1].width;
            m_skyline.erase(m_skyline.begin() + k + 1);
        }
        else
        {
            ++k;
        }
    }

    return true;
}

}  // namespace render
}  // namespace kryga

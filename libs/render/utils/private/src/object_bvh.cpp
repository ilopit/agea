#include "render/utils/object_bvh.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace kryga
{
namespace render
{

namespace
{

// Ray-AABB intersection (slab method). Returns t of entry point, or -1 on miss.
float
ray_aabb(const ray& r,
         const glm::vec3& inv_dir,
         const glm::vec3& aabb_min,
         const glm::vec3& aabb_max)
{
    glm::vec3 t0 = (aabb_min - r.origin) * inv_dir;
    glm::vec3 t1 = (aabb_max - r.origin) * inv_dir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float enter = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float exit_ = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

    if (enter > exit_ || exit_ < 0.0f)
    {
        return -1.0f;
    }

    // Return entry point (or 0 if inside the box)
    return enter > 0.0f ? enter : 0.0f;
}

glm::vec3
entry_center(const bvh_object_entry& e)
{
    return (e.aabb_min + e.aabb_max) * 0.5f;
}

}  // namespace

// ============================================================================
// Build
// ============================================================================

void
object_bvh::clear()
{
    m_nodes.clear();
    m_objects.clear();
}

void
object_bvh::build(const bvh_object_entry* entries, uint32_t count)
{
    clear();

    if (count == 0)
    {
        return;
    }

    m_objects.assign(entries, entries + count);
    m_nodes.reserve(count * 2);
    m_nodes.push_back({});
    build_recursive(0, 0, count);
}

void
object_bvh::build_recursive(uint32_t node_idx, uint32_t begin, uint32_t end)
{
    auto& node = m_nodes[node_idx];
    uint32_t count = end - begin;

    // Compute AABB for this range
    glm::vec3 bb_min(std::numeric_limits<float>::max());
    glm::vec3 bb_max(std::numeric_limits<float>::lowest());
    for (uint32_t i = begin; i < end; ++i)
    {
        bb_min = glm::min(bb_min, m_objects[i].aabb_min);
        bb_max = glm::max(bb_max, m_objects[i].aabb_max);
    }
    node.aabb_min = bb_min;
    node.aabb_max = bb_max;

    if (count <= 2)
    {
        node.left_or_first = begin;
        node.count = count;
        return;
    }

    // Split along longest axis at median
    glm::vec3 extent = bb_max - bb_min;
    int axis = 0;
    if (extent.y > extent.x)
    {
        axis = 1;
    }
    if (extent.z > extent[axis])
    {
        axis = 2;
    }

    uint32_t mid = begin + count / 2;
    std::nth_element(m_objects.begin() + begin,
                     m_objects.begin() + mid,
                     m_objects.begin() + end,
                     [axis](const bvh_object_entry& a, const bvh_object_entry& b)
                     { return entry_center(a)[axis] < entry_center(b)[axis]; });

    node.count = 0;
    uint32_t left_idx = static_cast<uint32_t>(m_nodes.size());
    m_nodes.push_back({});
    m_nodes.push_back({});
    m_nodes[node_idx].left_or_first = left_idx;

    build_recursive(left_idx, begin, mid);
    build_recursive(left_idx + 1, mid, end);
}

// ============================================================================
// Raycast
// ============================================================================

bool
object_bvh::raycast(const ray& r, raycast_hit& out_hit) const
{
    if (m_nodes.empty())
    {
        return false;
    }

    glm::vec3 inv_dir = 1.0f / r.direction;
    float best_t = std::numeric_limits<float>::max();
    bool hit = false;

    constexpr uint32_t STACK_SIZE = 32;
    uint32_t stack[STACK_SIZE];
    int stack_ptr = 1;
    stack[0] = 0;

    while (stack_ptr > 0)
    {
        uint32_t idx = stack[--stack_ptr];
        const auto& node = m_nodes[idx];

        // Test ray vs node AABB
        float node_t = ray_aabb(r, inv_dir, node.aabb_min, node.aabb_max);
        if (node_t < 0.0f || node_t >= best_t)
        {
            continue;
        }

        if (node.count > 0)
        {
            // Leaf — test objects
            for (uint32_t i = 0; i < node.count; ++i)
            {
                const auto& obj = m_objects[node.left_or_first + i];
                float t = ray_aabb(r, inv_dir, obj.aabb_min, obj.aabb_max);
                if (t >= 0.0f && t < best_t)
                {
                    best_t = t;
                    out_hit.distance = t;
                    out_hit.user_id = obj.user_id;
                    out_hit.user_data = obj.user_data;
                    hit = true;
                }
            }
        }
        else
        {
            if (stack_ptr + 2 <= STACK_SIZE)
            {
                stack[stack_ptr++] = node.left_or_first + 1;
                stack[stack_ptr++] = node.left_or_first;
            }
        }
    }

    return hit;
}

// ============================================================================
// Screen to Ray
// ============================================================================

ray
object_bvh::screen_to_ray(uint32_t screen_x,
                          uint32_t screen_y,
                          uint32_t screen_w,
                          uint32_t screen_h,
                          const glm::mat4& inv_projection,
                          const glm::mat4& inv_view)
{
    float ndc_x = (2.0f * screen_x / screen_w) - 1.0f;
    float ndc_y = (2.0f * screen_y / screen_h) - 1.0f;

    glm::vec4 near_clip(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 far_clip(ndc_x, ndc_y, 1.0f, 1.0f);

    glm::vec4 near_view = inv_projection * near_clip;
    glm::vec4 far_view = inv_projection * far_clip;
    near_view /= near_view.w;
    far_view /= far_view.w;

    glm::vec3 dir_view = glm::normalize(glm::vec3(far_view) - glm::vec3(near_view));

    glm::vec3 origin = glm::vec3(inv_view * glm::vec4(glm::vec3(near_view), 1.0f));
    glm::vec3 direction = glm::normalize(glm::mat3(inv_view) * dir_view);

    return {origin, direction};
}

}  // namespace render
}  // namespace kryga

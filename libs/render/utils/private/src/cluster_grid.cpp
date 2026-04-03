#include "render/utils/cluster_grid.h"

#include <algorithm>
#include <limits>

namespace kryga
{
namespace render
{

void
cluster_grid::init(uint32_t screen_width,
                   uint32_t screen_height,
                   float near_plane,
                   float far_plane,
                   uint32_t tile_size,
                   uint32_t depth_slices,
                   uint32_t max_lights_per_cluster)
{
    m_config.tile_size = tile_size;
    m_config.depth_slices = depth_slices;
    m_config.max_lights_per_cluster = max_lights_per_cluster;
    m_config.near_plane = near_plane;
    m_config.far_plane = far_plane;

    m_config.update_dimensions(screen_width, screen_height);

    // Allocate storage
    m_cluster_light_counts.resize(m_config.total_clusters, 0);
    m_cluster_light_indices.resize(m_config.total_clusters * m_config.max_lights_per_cluster, 0);
    m_cluster_aabbs.resize(m_config.total_clusters);

    m_aabbs_dirty = true;
    m_initialized = true;
}

void
cluster_grid::resize(uint32_t screen_width, uint32_t screen_height)
{
    if (!m_initialized)
    {
        return;
    }

    m_config.update_dimensions(screen_width, screen_height);

    m_cluster_light_counts.resize(m_config.total_clusters, 0);
    m_cluster_light_indices.resize(m_config.total_clusters * m_config.max_lights_per_cluster, 0);
    m_cluster_aabbs.resize(m_config.total_clusters);

    m_aabbs_dirty = true;
}

void
cluster_grid::set_planes(float near_plane, float far_plane)
{
    if (m_config.near_plane != near_plane || m_config.far_plane != far_plane)
    {
        m_config.near_plane = near_plane;
        m_config.far_plane = far_plane;
        m_aabbs_dirty = true;
    }
}

void
cluster_grid::clear()
{
    std::fill(m_cluster_light_counts.begin(), m_cluster_light_counts.end(), 0);
    m_active_clusters = 0;
    m_total_light_assignments = 0;
}

float
cluster_grid::slice_to_depth(uint32_t slice) const
{
    // Logarithmic distribution: more slices near camera
    // depth = near * (far/near)^(slice/num_slices)
    const float ratio = m_config.far_plane / m_config.near_plane;
    const float t = static_cast<float>(slice) / static_cast<float>(m_config.depth_slices);
    return m_config.near_plane * std::pow(ratio, t);
}

uint32_t
cluster_grid::depth_to_slice(float depth) const
{
    if (depth <= m_config.near_plane)
    {
        return 0;
    }
    float log_ratio = std::log(m_config.far_plane / m_config.near_plane);
    float log_depth = std::log(depth / m_config.near_plane);
    float t = log_depth / log_ratio;

    t = std::clamp(t, 0.0f, 0.99999994f);

    uint32_t slice = uint32_t(t * m_config.depth_slices);
    return std::min(slice, m_config.depth_slices - 1);
}

uint32_t
cluster_grid::get_cluster_index(uint32_t tile_x, uint32_t tile_y, uint32_t slice) const
{
    // Layout: slice-major for better cache coherence during light assignment
    return slice * (m_config.tiles_x * m_config.tiles_y) + tile_y * m_config.tiles_x + tile_x;
}

uint32_t
cluster_grid::get_cluster_index(uint32_t screen_x, uint32_t screen_y, float view_depth) const
{
    const uint32_t tile_x = screen_x / m_config.tile_size;
    const uint32_t tile_y = screen_y / m_config.tile_size;
    const uint32_t slice = depth_to_slice(view_depth);
    return get_cluster_index(tile_x, tile_y, slice);
}

uint32_t
cluster_grid::get_depth_slice(float view_depth) const
{
    return depth_to_slice(view_depth);
}

glm::vec3
cluster_grid::screen_to_view(float screen_x,
                             float screen_y,
                             float depth,
                             const glm::mat4& inv_projection) const
{
    // Convert screen coords to NDC [-1, 1]
    // Use actual screen dimensions for correct mapping
    const float ndc_x = (2.0f * screen_x / static_cast<float>(m_config.screen_width)) - 1.0f;
    // Vulkan: NDC y=-1 at screen top (y=0), NDC y=+1 at screen bottom
    const float ndc_y = (2.0f * screen_y / static_cast<float>(m_config.screen_height)) - 1.0f;

    // Create a point on the near plane in clip space, then unproject
    glm::vec4 clip_point(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 view_point = inv_projection * clip_point;
    view_point /= view_point.w;

    // Scale to desired depth
    // Note: view_point.z is negative (OpenGL convention), depth is positive
    // We want AABBs in positive-Z-forward space, so use abs
    const float scale = depth / std::abs(view_point.z);
    return glm::vec3(view_point.x * scale, view_point.y * scale, depth);
}

cluster_aabb
cluster_grid::compute_cluster_aabb(uint32_t tile_x,
                                   uint32_t tile_y,
                                   uint32_t slice,
                                   const glm::mat4& inv_projection) const
{
    // Screen-space bounds of this tile
    const float min_x = static_cast<float>(tile_x * m_config.tile_size);
    const float max_x = static_cast<float>((tile_x + 1) * m_config.tile_size);
    const float min_y = static_cast<float>(tile_y * m_config.tile_size);
    const float max_y = static_cast<float>((tile_y + 1) * m_config.tile_size);

    // Depth bounds of this slice
    const float near_depth = slice_to_depth(slice);
    const float far_depth = slice_to_depth(slice + 1);

    // Compute 8 corners of the cluster frustum in view space
    glm::vec3 corners[8];
    corners[0] = screen_to_view(min_x, min_y, near_depth, inv_projection);
    corners[1] = screen_to_view(max_x, min_y, near_depth, inv_projection);
    corners[2] = screen_to_view(min_x, max_y, near_depth, inv_projection);
    corners[3] = screen_to_view(max_x, max_y, near_depth, inv_projection);
    corners[4] = screen_to_view(min_x, min_y, far_depth, inv_projection);
    corners[5] = screen_to_view(max_x, min_y, far_depth, inv_projection);
    corners[6] = screen_to_view(min_x, max_y, far_depth, inv_projection);
    corners[7] = screen_to_view(max_x, max_y, far_depth, inv_projection);

    // Compute AABB from corners
    cluster_aabb aabb;
    aabb.min_point = corners[0];
    aabb.max_point = corners[0];

    for (int i = 1; i < 8; ++i)
    {
        aabb.min_point = glm::min(aabb.min_point, corners[i]);
        aabb.max_point = glm::max(aabb.max_point, corners[i]);
    }

    return aabb;
}

bool
cluster_grid::sphere_intersects_aabb(const glm::vec3& center,
                                     float radius,
                                     const cluster_aabb& aabb) const
{
    // Find closest point on AABB to sphere center
    const glm::vec3 closest = glm::clamp(center, aabb.min_point, aabb.max_point);

    // Check if distance to closest point is within radius
    const glm::vec3 diff = center - closest;
    const float dist_sq = glm::dot(diff, diff);

    return dist_sq <= (radius * radius);
}

void
cluster_grid::build_clusters(const glm::mat4& view,
                             const glm::mat4& projection,
                             const glm::mat4& inv_projection,
                             const std::vector<cluster_light_info>& lights)
{
    // Clear previous frame's data
    clear();

    // Rebuild AABBs if projection changed
    if (m_aabbs_dirty || m_cached_inv_projection != inv_projection)
    {
        m_cached_inv_projection = inv_projection;

        for (uint32_t slice = 0; slice < m_config.depth_slices; ++slice)
        {
            for (uint32_t tile_y = 0; tile_y < m_config.tiles_y; ++tile_y)
            {
                for (uint32_t tile_x = 0; tile_x < m_config.tiles_x; ++tile_x)
                {
                    const uint32_t idx = get_cluster_index(tile_x, tile_y, slice);
                    m_cluster_aabbs[idx] =
                        compute_cluster_aabb(tile_x, tile_y, slice, inv_projection);
                }
            }
        }
        m_aabbs_dirty = false;
    }

    // Assign each light to overlapping clusters
    for (const auto& light : lights)
    {
        // Transform light position to view space
        const glm::vec4 view_pos = view * glm::vec4(light.position, 1.0f);
        const glm::vec3 light_view_pos = glm::vec3(view_pos);
        const float light_depth =
            -light_view_pos.z;  // Negate: OpenGL view space Z is negative forward

        // Skip lights behind camera or beyond far plane
        if (light_depth + light.radius < m_config.near_plane)
        {
            continue;
        }

        if (light_depth - light.radius > m_config.far_plane)
        {
            continue;
        }

        // Find depth slice range this light can affect
        const uint32_t min_slice =
            depth_to_slice(std::max(light_depth - light.radius, m_config.near_plane));
        const uint32_t max_slice =
            std::min(depth_to_slice(light_depth + light.radius) + 1, m_config.depth_slices);

        // Light position with corrected Z for AABB test (AABBs use positive Z forward)
        const glm::vec3 light_view_pos_corrected(light_view_pos.x, light_view_pos.y, light_depth);

        // For each potentially affected cluster, do precise intersection test
        for (uint32_t slice = min_slice; slice < max_slice; ++slice)
        {
            for (uint32_t tile_y = 0; tile_y < m_config.tiles_y; ++tile_y)
            {
                for (uint32_t tile_x = 0; tile_x < m_config.tiles_x; ++tile_x)
                {
                    const uint32_t cluster_idx = get_cluster_index(tile_x, tile_y, slice);
                    const cluster_aabb& aabb = m_cluster_aabbs[cluster_idx];

                    if (sphere_intersects_aabb(light_view_pos_corrected, light.radius, aabb))
                    {
                        uint32_t& count = m_cluster_light_counts[cluster_idx];
                        if (count < m_config.max_lights_per_cluster)
                        {
                            const uint32_t light_list_idx =
                                cluster_idx * m_config.max_lights_per_cluster + count;
                            m_cluster_light_indices[light_list_idx] = light.slot;
                            ++count;
                            ++m_total_light_assignments;

                            if (count == 1)
                            {
                                ++m_active_clusters;
                            }
                        }
                    }
                }
            }
        }
    }
}

}  // namespace render
}  // namespace kryga

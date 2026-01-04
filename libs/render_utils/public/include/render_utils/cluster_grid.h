#pragma once

#include <glm_unofficial/glm.h>

#include <vector>
#include <cstdint>
#include <cmath>

namespace kryga
{
namespace render
{

// Configuration for clustered lighting grid
struct cluster_grid_config
{
    uint32_t tile_size = 64;     // Screen-space tile size in pixels
    uint32_t depth_slices = 24;  // Number of depth slices
    uint32_t max_lights_per_cluster = 32;

    float near_plane = 0.1f;
    float far_plane = 1000.0f;

    // Computed values (call update_dimensions after changing screen size)
    uint32_t tiles_x = 0;
    uint32_t tiles_y = 0;
    uint32_t total_clusters = 0;

    void
    update_dimensions(uint32_t screen_width, uint32_t screen_height)
    {
        tiles_x = (screen_width + tile_size - 1) / tile_size;
        tiles_y = (screen_height + tile_size - 1) / tile_size;
        total_clusters = tiles_x * tiles_y * depth_slices;
    }
};

// Light data for cluster assignment
struct cluster_light_info
{
    uint32_t slot;       // Index into universal lights SSBO
    glm::vec3 position;  // World position
    float radius;        // Effective radius
};

// Cluster bounds in view space
struct cluster_aabb
{
    glm::vec3 min_point;
    glm::vec3 max_point;
};

class cluster_grid
{
public:
    static constexpr uint32_t DEFAULT_TILE_SIZE = 64;
    static constexpr uint32_t DEFAULT_DEPTH_SLICES = 24;
    static constexpr uint32_t DEFAULT_MAX_LIGHTS = 32;

    cluster_grid() = default;

    // Initialize with screen dimensions and camera planes
    void
    init(uint32_t screen_width,
         uint32_t screen_height,
         float near_plane,
         float far_plane,
         uint32_t tile_size = DEFAULT_TILE_SIZE,
         uint32_t depth_slices = DEFAULT_DEPTH_SLICES,
         uint32_t max_lights_per_cluster = DEFAULT_MAX_LIGHTS);

    // Reinitialize on screen resize
    void
    resize(uint32_t screen_width, uint32_t screen_height);

    // Update camera planes if they change
    void
    set_planes(float near_plane, float far_plane);

    // Clear all cluster light assignments
    void
    clear();

    // Assign lights to clusters based on current view/projection
    // Call once per frame before drawing
    void
    build_clusters(const glm::mat4& view,
                   const glm::mat4& projection,
                   const glm::mat4& inv_projection,
                   const std::vector<cluster_light_info>& lights);

    // Get cluster index from screen position and view-space depth
    uint32_t
    get_cluster_index(uint32_t screen_x, uint32_t screen_y, float view_depth) const;

    // Get cluster index from tile coordinates and depth slice
    uint32_t
    get_cluster_index(uint32_t tile_x, uint32_t tile_y, uint32_t slice) const;

    // Get depth slice from linear view-space depth
    uint32_t
    get_depth_slice(float view_depth) const;

    // Access cluster data for GPU upload
    const std::vector<uint32_t>&
    get_cluster_light_counts() const
    {
        return m_cluster_light_counts;
    }

    // Flat array: for each cluster, up to max_lights_per_cluster light indices
    const std::vector<uint32_t>&
    get_cluster_light_indices() const
    {
        return m_cluster_light_indices;
    }

    const cluster_grid_config&
    get_config() const
    {
        return m_config;
    }

    bool
    is_initialized() const
    {
        return m_initialized;
    }

    // Statistics
    uint32_t
    get_active_clusters() const
    {
        return m_active_clusters;
    }

    uint32_t
    get_total_light_assignments() const
    {
        return m_total_light_assignments;
    }

private:
    // Compute AABB of a cluster in view space
    cluster_aabb
    compute_cluster_aabb(uint32_t tile_x,
                         uint32_t tile_y,
                         uint32_t slice,
                         const glm::mat4& inv_projection) const;

    // Test sphere-AABB intersection
    bool
    sphere_intersects_aabb(const glm::vec3& center, float radius, const cluster_aabb& aabb) const;

    // Convert screen-space point to view-space at given depth
    glm::vec3
    screen_to_view(float screen_x,
                   float screen_y,
                   float depth,
                   const glm::mat4& inv_projection) const;

    // Logarithmic depth slice distribution
    float
    slice_to_depth(uint32_t slice) const;

    uint32_t
    depth_to_slice(float depth) const;

    cluster_grid_config m_config;

    // Per-cluster light count
    std::vector<uint32_t> m_cluster_light_counts;

    // Flat array of light indices per cluster
    // Layout: [cluster0_light0, cluster0_light1, ..., cluster1_light0, ...]
    std::vector<uint32_t> m_cluster_light_indices;

    // Precomputed cluster AABBs in view space (recomputed on projection change)
    std::vector<cluster_aabb> m_cluster_aabbs;

    // Cached inverse projection for AABB computation
    glm::mat4 m_cached_inv_projection = glm::mat4(1.0f);

    // Statistics
    uint32_t m_active_clusters = 0;
    uint32_t m_total_light_assignments = 0;

    bool m_initialized = false;
    bool m_aabbs_dirty = true;
};

}  // namespace render
}  // namespace kryga

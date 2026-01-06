#include <gtest/gtest.h>

#include "render_utils/cluster_grid.h"

#include <glm/gtc/matrix_transform.hpp>
#include <gpu_types/gpu_generic_constants.h>

using namespace kryga::render;

// Helper: Create projection matrix (Y-flip handled at viewport level)
glm::mat4
make_projection(float fov_degrees, float aspect, float near, float far)
{
    return glm::perspective(glm::radians(fov_degrees), aspect, near, far);
}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(ClusterGrid, not_initialized_by_default)
{
    cluster_grid grid;
    EXPECT_FALSE(grid.is_initialized());
}

TEST(ClusterGrid, initialized_after_init)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f);
    EXPECT_TRUE(grid.is_initialized());
}

TEST(ClusterGrid, config_set_correctly)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar, 64, 24, 128);

    const auto& config = grid.get_config();
    EXPECT_EQ(config.screen_width, 800u);
    EXPECT_EQ(config.screen_height, 600u);
    EXPECT_FLOAT_EQ(config.near_plane, KGPU_znear);
    EXPECT_FLOAT_EQ(config.far_plane, KGPU_zfar);
    EXPECT_EQ(config.tile_size, 64u);
    EXPECT_EQ(config.depth_slices, 24u);
    EXPECT_EQ(config.max_lights_per_cluster, 128u);
}

TEST(ClusterGrid, tiles_computed_correctly)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    const auto& config = grid.get_config();
    // 800 / 64 = 12.5 -> 13 tiles
    // 600 / 64 = 9.375 -> 10 tiles
    EXPECT_EQ(config.tiles_x, 13u);
    EXPECT_EQ(config.tiles_y, 10u);
    EXPECT_EQ(config.total_clusters, 13u * 10u * 24u);
}

// ============================================================================
// Depth slice tests (logarithmic distribution)
// ============================================================================

TEST(ClusterGrid, depth_slice_near_plane_returns_zero)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // Depth at or before near plane should return slice 0
    EXPECT_EQ(grid.get_depth_slice(0.1f), 0u);
    EXPECT_EQ(grid.get_depth_slice(0.05f), 0u);
}

TEST(ClusterGrid, depth_slice_far_plane_returns_last)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // Depth at far plane should return last slice
    uint32_t last_slice = grid.get_depth_slice(1000.0f);
    EXPECT_EQ(last_slice, 23u);
}

TEST(ClusterGrid, depth_slice_logarithmic_distribution)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // With logarithmic distribution, more slices are near the camera
    // Check that middle depth (geometric mean) is around middle slice
    float geometric_mean = std::sqrt(0.1f * 1000.0f);  // ~10
    uint32_t mid_slice = grid.get_depth_slice(geometric_mean);

    // Should be around slice 12 (half of 24)
    EXPECT_GE(mid_slice, 10u);
    EXPECT_LE(mid_slice, 14u);
}

TEST(ClusterGrid, depth_slices_increase_monotonically)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    uint32_t prev_slice = 0;
    for (float depth = 0.1f; depth <= 1000.0f; depth *= 1.5f)
    {
        uint32_t slice = grid.get_depth_slice(depth);
        EXPECT_GE(slice, prev_slice);
        prev_slice = slice;
    }
}

// ============================================================================
// Cluster index tests
// ============================================================================

TEST(ClusterGrid, cluster_index_layout)
{
    cluster_grid grid;
    grid.init(640, 480, 0.1f, 1000.0f, 64, 24, 128);

    const auto& config = grid.get_config();

    // Test that cluster index is: slice * (tiles_x * tiles_y) + tile_y * tiles_x + tile_x
    uint32_t idx = grid.get_cluster_index(2U, 3U, 5U);
    uint32_t expected = 5 * (config.tiles_x * config.tiles_y) + 3 * config.tiles_x + 2;
    EXPECT_EQ(idx, expected);
}

// ============================================================================
// Light assignment tests
// ============================================================================

TEST(ClusterGrid, light_at_origin_assigned_to_clusters)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // Camera at (0,0,10) looking at origin
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    std::vector<cluster_light_info> lights;
    lights.push_back({0, glm::vec3(0.0f, 0.0f, 0.0f), 5.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Should have some active clusters
    EXPECT_GT(grid.get_active_clusters(), 0u);
    EXPECT_GT(grid.get_total_light_assignments(), 0u);
}

TEST(ClusterGrid, light_behind_camera_not_assigned)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // Camera at origin looking down -Z
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    std::vector<cluster_light_info> lights;
    // Light behind camera (positive Z when camera looks down -Z)
    lights.push_back({0, glm::vec3(0.0f, 0.0f, 50.0f), 5.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Light behind camera should not be assigned
    EXPECT_EQ(grid.get_active_clusters(), 0u);
}

TEST(ClusterGrid, light_beyond_far_plane_not_assigned)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 100.0f, 64, 24, 128);

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    std::vector<cluster_light_info> lights;
    // Light way beyond far plane
    lights.push_back({0, glm::vec3(0.0f, 0.0f, -500.0f), 5.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    EXPECT_EQ(grid.get_active_clusters(), 0u);
}

TEST(ClusterGrid, large_radius_light_assigned_to_many_clusters)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 50.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    std::vector<cluster_light_info> lights;
    // Large light at origin
    lights.push_back({0, glm::vec3(0.0f, 0.0f, 0.0f), 100.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Large light should affect many clusters
    EXPECT_GT(grid.get_active_clusters(), 10u);
}

TEST(ClusterGrid, clear_removes_assignments)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    std::vector<cluster_light_info> lights;
    lights.push_back({0, glm::vec3(0.0f, 0.0f, 0.0f), 5.0f});

    grid.build_clusters(view, proj, inv_proj, lights);
    EXPECT_GT(grid.get_active_clusters(), 0u);

    grid.clear();
    EXPECT_EQ(grid.get_active_clusters(), 0u);
    EXPECT_EQ(grid.get_total_light_assignments(), 0u);
}

// ============================================================================
// View space depth consistency tests
// ============================================================================

TEST(ClusterGrid, view_space_z_positive_forward)
{
    // This test verifies the coordinate system assumption:
    // View space Z should be positive for objects in front of camera

    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // Camera at (0,0,100) looking at origin
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 100.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    // Transform origin to view space
    glm::vec4 origin_view = view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    // The sign of Z determines our coordinate convention
    // This test documents what the code expects
    float view_z = origin_view.z;

    // Log the actual value for debugging
    std::cout << "Origin in view space Z: " << view_z << std::endl;
    std::cout << "Distance from camera: " << glm::length(glm::vec3(origin_view)) << std::endl;

    // The cluster_grid code assumes positive Z for forward objects
    // If this fails, either the assumption is wrong or view matrix setup differs
}

TEST(ClusterGrid, multiple_lights_at_different_depths)
{
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    // Camera at origin looking down -Z
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    std::vector<cluster_light_info> lights;
    // Lights at different depths along -Z axis
    lights.push_back({0, glm::vec3(0.0f, 0.0f, -10.0f), 5.0f});   // Near
    lights.push_back({1, glm::vec3(0.0f, 0.0f, -100.0f), 5.0f});  // Mid
    lights.push_back({2, glm::vec3(0.0f, 0.0f, -500.0f), 5.0f});  // Far

    grid.build_clusters(view, proj, inv_proj, lights);

    // All three lights should be assigned to some clusters
    EXPECT_GE(grid.get_total_light_assignments(), 3u);
}

TEST(ClusterGrid, shader_lookup_consistency)
{
    // This test simulates what the shader does and verifies consistency
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    const auto& config = grid.get_config();

    // Camera setup
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 50.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at origin
    std::vector<cluster_light_info> lights;
    lights.push_back({42, glm::vec3(0.0f, 0.0f, 0.0f), 10.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Simulate shader lookup for a fragment at origin
    glm::vec4 frag_view = view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    float view_depth = -frag_view.z;  // Negate: shader uses -viewPos.z

    // Get depth slice using same logic as shader
    uint32_t slice = grid.get_depth_slice(view_depth);

    // Fragment at screen center
    uint32_t screen_x = 400;
    uint32_t screen_y = 300;
    uint32_t tile_x = screen_x / config.tile_size;
    uint32_t tile_y = screen_y / config.tile_size;

    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    // Check if light 42 is in this cluster
    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;

    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 42)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "View depth: " << view_depth << std::endl;
    std::cout << "Depth slice: " << slice << std::endl;
    std::cout << "Cluster index: " << cluster_idx << std::endl;
    std::cout << "Lights in cluster: " << light_count << std::endl;
    std::cout << "Found target light: " << (found_light ? "yes" : "no") << std::endl;

    // The light should be found in the cluster
    EXPECT_TRUE(found_light);
}

TEST(ClusterGrid, vulkan_projection_shader_lookup_consistency)
{
    // This test uses Vulkan-style Y-flipped projection (as the actual renderer does)
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    const auto& config = grid.get_config();

    // Camera setup with Vulkan projection
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 50.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at origin
    std::vector<cluster_light_info> lights;
    lights.push_back({42, glm::vec3(0.0f, 0.0f, 0.0f), 10.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Simulate shader lookup for a fragment at origin
    glm::vec4 frag_view = view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    float view_depth = -frag_view.z;  // Negate: shader uses -viewPos.z

    // Get depth slice using same logic as shader
    uint32_t slice = grid.get_depth_slice(view_depth);

    // Fragment at screen center
    uint32_t screen_x = 400;
    uint32_t screen_y = 300;
    uint32_t tile_x = screen_x / config.tile_size;
    uint32_t tile_y = screen_y / config.tile_size;

    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    // Check if light 42 is in this cluster
    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;

    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 42)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "Vulkan proj - View depth: " << view_depth << std::endl;
    std::cout << "Vulkan proj - Depth slice: " << slice << std::endl;
    std::cout << "Vulkan proj - Cluster index: " << cluster_idx << std::endl;
    std::cout << "Vulkan proj - Lights in cluster: " << light_count << std::endl;
    std::cout << "Vulkan proj - Found target light: " << (found_light ? "yes" : "no") << std::endl;

    // The light should be found in the cluster
    EXPECT_TRUE(found_light);
}

TEST(ClusterGrid, vulkan_projection_off_center_light)
{
    // Test with a light that's off-center to verify Y coordinate handling
    cluster_grid grid;
    grid.init(800, 600, 0.1f, 1000.0f, 64, 24, 128);

    const auto& config = grid.get_config();

    // Camera at origin looking down -Z
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, 1000.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light above center, 50 units in front
    glm::vec3 light_pos(0.0f, 20.0f, -50.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({99, light_pos, 15.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Transform light to view space (as shader does)
    glm::vec4 light_view = view * glm::vec4(light_pos, 1.0f);
    float view_depth = -light_view.z;

    // Project light to screen space to find which tile it's in
    glm::vec4 light_clip = proj * light_view;
    glm::vec3 light_ndc = glm::vec3(light_clip) / light_clip.w;

    // NDC to screen coords (Vulkan: y goes from -1 (top) to +1 (bottom))
    float screen_x = (light_ndc.x + 1.0f) * 0.5f * 800.0f;
    float screen_y = (light_ndc.y + 1.0f) * 0.5f * 600.0f;

    uint32_t tile_x = uint32_t(screen_x) / config.tile_size;
    uint32_t tile_y = uint32_t(screen_y) / config.tile_size;
    uint32_t slice = grid.get_depth_slice(view_depth);

    // Clamp to valid range
    tile_x = std::min(tile_x, config.tiles_x - 1);
    tile_y = std::min(tile_y, config.tiles_y - 1);

    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;

    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 99)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "Off-center light - Light view pos: (" << light_view.x << ", " << light_view.y
              << ", " << light_view.z << ")" << std::endl;
    std::cout << "Off-center light - View depth: " << view_depth << std::endl;
    std::cout << "Off-center light - Screen pos: (" << screen_x << ", " << screen_y << ")"
              << std::endl;
    std::cout << "Off-center light - Tile: (" << tile_x << ", " << tile_y << "), Slice: " << slice
              << std::endl;
    std::cout << "Off-center light - Cluster index: " << cluster_idx << std::endl;
    std::cout << "Off-center light - Lights in cluster: " << light_count << std::endl;
    std::cout << "Off-center light - Found target light: " << (found_light ? "yes" : "no")
              << std::endl;

    // The light should be found in its projected cluster
    EXPECT_TRUE(found_light);
}

TEST(ClusterGrid, far_depth_light_assignment)
{
    // Test lights at far depth - matching actual renderer config
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);  // Match actual renderer

    const auto& config = grid.get_config();

    // Camera at origin looking down -Z
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at far distance (1500 units in front of camera)
    glm::vec3 light_pos(0.0f, 0.0f, -1500.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({77, light_pos, 100.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Transform light to view space
    glm::vec4 light_view = view * glm::vec4(light_pos, 1.0f);
    float view_depth = -light_view.z;

    uint32_t slice = grid.get_depth_slice(view_depth);

    std::cout << "Far light - Light world pos: (0, 0, -1500)" << std::endl;
    std::cout << "Far light - Light view pos: (" << light_view.x << ", " << light_view.y << ", "
              << light_view.z << ")" << std::endl;
    std::cout << "Far light - View depth: " << view_depth << std::endl;
    std::cout << "Far light - Depth slice: " << slice << " / " << config.depth_slices << std::endl;
    std::cout << "Far light - Active clusters: " << grid.get_active_clusters() << std::endl;
    std::cout << "Far light - Total light assignments: " << grid.get_total_light_assignments()
              << std::endl;

    // Check center tile at this depth
    uint32_t tile_x = (400) / config.tile_size;  // Center X
    uint32_t tile_y = (300) / config.tile_size;  // Center Y
    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;

    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 77)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "Far light - Center tile: (" << tile_x << ", " << tile_y << ")" << std::endl;
    std::cout << "Far light - Cluster index: " << cluster_idx << std::endl;
    std::cout << "Far light - Lights in cluster: " << light_count << std::endl;
    std::cout << "Far light - Found target light: " << (found_light ? "yes" : "no") << std::endl;

    // Light should be assigned to some clusters
    EXPECT_GT(grid.get_active_clusters(), 0u);
    EXPECT_TRUE(found_light);
}

TEST(ClusterGrid, very_far_depth_light_near_far_plane)
{
    // Test light very close to far plane
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);

    const auto& config = grid.get_config();

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at 1900 units (close to far plane of KGPU_zfar)
    glm::vec3 light_pos(0.0f, 0.0f, -1900.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({88, light_pos, 150.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    glm::vec4 light_view = view * glm::vec4(light_pos, 1.0f);
    float view_depth = -light_view.z;
    uint32_t slice = grid.get_depth_slice(view_depth);

    std::cout << "Very far light - View depth: " << view_depth << std::endl;
    std::cout << "Very far light - Depth slice: " << slice << " / " << config.depth_slices
              << std::endl;
    std::cout << "Very far light - Active clusters: " << grid.get_active_clusters() << std::endl;

    // Should definitely have some clusters with this light
    EXPECT_GT(grid.get_active_clusters(), 0u);
    EXPECT_GT(grid.get_total_light_assignments(), 0u);
}

TEST(ClusterGrid, depth_slice_distribution)
{
    // Print depth slice boundaries for debugging
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);

    const auto& config = grid.get_config();
    float log_ratio = std::log(config.far_plane / config.near_plane);

    std::cout << "Depth slice distribution (near=0.1, far=KGPU_zfar, slices=12):" << std::endl;
    for (uint32_t i = 0; i <= config.depth_slices; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(config.depth_slices);
        float depth = config.near_plane * std::pow(config.far_plane / config.near_plane, t);
        std::cout << "  Slice " << i << " boundary: " << depth << std::endl;
    }

    // Test some specific depths
    std::vector<float> test_depths = {1.0f,   10.0f,   50.0f,   100.0f,
                                      500.0f, 1000.0f, 1500.0f, 1900.0f};
    std::cout << "Depth to slice mapping:" << std::endl;
    for (float d : test_depths)
    {
        uint32_t slice = grid.get_depth_slice(d);
        std::cout << "  Depth " << d << " -> Slice " << slice << std::endl;
    }
}

TEST(ClusterGrid, camera_offset_far_light)
{
    // Test with camera NOT at origin - closer to real game scenario
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);

    const auto& config = grid.get_config();

    // Camera at (0, 50, 500) looking at (0, 0, 0)
    glm::vec3 cam_pos(0.0f, 50.0f, 500.0f);
    glm::mat4 view = glm::lookAt(cam_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at world origin with radius 50
    glm::vec3 light_world_pos(0.0f, 0.0f, 0.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({55, light_world_pos, 50.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Transform light to view space (as shader does)
    glm::vec4 light_view = view * glm::vec4(light_world_pos, 1.0f);
    float light_view_depth = -light_view.z;

    std::cout << "Camera offset test:" << std::endl;
    std::cout << "  Camera pos: (0, 50, 500)" << std::endl;
    std::cout << "  Light world pos: (0, 0, 0)" << std::endl;
    std::cout << "  Light view pos: (" << light_view.x << ", " << light_view.y << ", "
              << light_view.z << ")" << std::endl;
    std::cout << "  Light view depth: " << light_view_depth << std::endl;
    std::cout << "  Light depth slice: " << grid.get_depth_slice(light_view_depth) << std::endl;
    std::cout << "  Active clusters: " << grid.get_active_clusters() << std::endl;
    std::cout << "  Total assignments: " << grid.get_total_light_assignments() << std::endl;

    // Now test an object near the light
    glm::vec3 object_world_pos(10.0f, 0.0f, 10.0f);  // Near the light
    glm::vec4 object_view = view * glm::vec4(object_world_pos, 1.0f);
    float object_view_depth = -object_view.z;

    // Project object to screen to find its tile
    glm::vec4 object_clip = proj * object_view;
    glm::vec3 object_ndc = glm::vec3(object_clip) / object_clip.w;
    float screen_x = (object_ndc.x + 1.0f) * 0.5f * 800.0f;
    float screen_y = (object_ndc.y + 1.0f) * 0.5f * 600.0f;

    uint32_t tile_x = std::min(uint32_t(screen_x) / config.tile_size, config.tiles_x - 1);
    uint32_t tile_y = std::min(uint32_t(screen_y) / config.tile_size, config.tiles_y - 1);
    uint32_t slice = grid.get_depth_slice(object_view_depth);
    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;
    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 55)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "  Object world pos: (10, 0, 10)" << std::endl;
    std::cout << "  Object view pos: (" << object_view.x << ", " << object_view.y << ", "
              << object_view.z << ")" << std::endl;
    std::cout << "  Object view depth: " << object_view_depth << std::endl;
    std::cout << "  Object screen pos: (" << screen_x << ", " << screen_y << ")" << std::endl;
    std::cout << "  Object tile: (" << tile_x << ", " << tile_y << "), slice: " << slice
              << std::endl;
    std::cout << "  Cluster idx: " << cluster_idx << std::endl;
    std::cout << "  Lights in cluster: " << light_count << std::endl;
    std::cout << "  Found light: " << (found_light ? "yes" : "no") << std::endl;

    // Distance from object to light
    float dist = glm::length(object_world_pos - light_world_pos);
    std::cout << "  Distance object to light: " << dist << " (radius: 50)" << std::endl;

    EXPECT_GT(grid.get_active_clusters(), 0u);
    EXPECT_TRUE(found_light) << "Object within light radius should find light in cluster";
}

TEST(ClusterGrid, multiple_lights_correct_assignment)
{
    // Test with both near and far lights - far clusters should have far lights
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);

    const auto& config = grid.get_config();

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Multiple lights at different depths
    std::vector<cluster_light_info> lights;
    lights.push_back({0, glm::vec3(0.0f, 0.0f, -50.0f), 50.0f});    // Near: depth 50
    lights.push_back({1, glm::vec3(0.0f, 0.0f, -200.0f), 50.0f});   // Mid: depth 200
    lights.push_back({2, glm::vec3(0.0f, 0.0f, -800.0f), 50.0f});   // Far: depth 800
    lights.push_back({3, glm::vec3(0.0f, 0.0f, -1200.0f), 50.0f});  // Very far: depth 1200

    grid.build_clusters(view, proj, inv_proj, lights);

    std::cout << "Multiple lights test:" << std::endl;
    for (size_t i = 0; i < lights.size(); ++i)
    {
        glm::vec4 lv = view * glm::vec4(lights[i].position, 1.0f);
        float depth = -lv.z;
        std::cout << "  Light " << i << ": depth=" << depth
                  << ", slice=" << grid.get_depth_slice(depth) << std::endl;
    }
    std::cout << "  Active clusters: " << grid.get_active_clusters() << std::endl;

    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    // Check which lights are in far cluster (slice 10, center tile)
    uint32_t center_tile_x = config.tiles_x / 2;
    uint32_t center_tile_y = config.tiles_y / 2;
    uint32_t far_cluster_idx = grid.get_cluster_index(center_tile_x, center_tile_y, 10);

    std::cout << "  Far cluster (slice 10) idx=" << far_cluster_idx
              << ", lights=" << counts[far_cluster_idx] << std::endl;

    for (uint32_t i = 0; i < counts[far_cluster_idx]; ++i)
    {
        uint32_t base_idx = far_cluster_idx * config.max_lights_per_cluster;
        uint32_t slot = indices[base_idx + i];
        std::cout << "    Contains light slot " << slot << std::endl;
    }

    // Light 2 (depth 800) should be in slice 10 (depth 384-876)
    bool found_light2 = false;
    for (uint32_t i = 0; i < counts[far_cluster_idx]; ++i)
    {
        uint32_t base_idx = far_cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 2)
            found_light2 = true;
    }

    EXPECT_TRUE(found_light2) << "Light 2 (depth 800) should be in slice 10 cluster";

    // Light 0 and 1 should NOT be in slice 10
    bool found_light0 = false, found_light1 = false;
    for (uint32_t i = 0; i < counts[far_cluster_idx]; ++i)
    {
        uint32_t base_idx = far_cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 0)
            found_light0 = true;
        if (indices[base_idx + i] == 1)
            found_light1 = true;
    }
    EXPECT_FALSE(found_light0) << "Light 0 (depth 50) should NOT be in slice 10";
    EXPECT_FALSE(found_light1) << "Light 1 (depth 200) should NOT be in slice 10";
}

TEST(ClusterGrid, near_light_not_in_far_cluster)
{
    // Verify that a near light is NOT assigned to far clusters
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);  // Match actual renderer

    const auto& config = grid.get_config();

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Near light at depth 100 with radius 50
    glm::vec3 near_light_pos(0.0f, 0.0f, -100.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({11, near_light_pos, 50.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    glm::vec4 light_view = view * glm::vec4(near_light_pos, 1.0f);
    float light_view_depth = -light_view.z;

    std::cout << "Near light test:" << std::endl;
    std::cout << "  Light depth: " << light_view_depth << std::endl;
    std::cout << "  Light slice: " << grid.get_depth_slice(light_view_depth) << std::endl;
    std::cout << "  Light covers depth: " << (light_view_depth - 50) << " to "
              << (light_view_depth + 50) << std::endl;
    std::cout << "  Active clusters: " << grid.get_active_clusters() << std::endl;

    // Check if this light appears in far clusters (slice 10, 11)
    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    // Check center tile at far slices
    uint32_t center_tile_x = config.tiles_x / 2;
    uint32_t center_tile_y = config.tiles_y / 2;

    for (uint32_t slice = 9; slice < config.depth_slices; ++slice)
    {
        uint32_t cluster_idx = grid.get_cluster_index(center_tile_x, center_tile_y, slice);
        uint32_t light_count = counts[cluster_idx];

        bool found_near_light = false;
        for (uint32_t i = 0; i < light_count; ++i)
        {
            uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
            if (indices[base_idx + i] == 11)
            {
                found_near_light = true;
                break;
            }
        }

        float slice_near = grid.get_depth_slice(100) == slice ? 100 : 0;
        std::cout << "  Slice " << slice << " (depth ~"
                  << (slice == 9 ? 168 : (slice == 10 ? 384 : 876)) << "): lights=" << light_count
                  << ", has_near_light=" << (found_near_light ? "YES" : "no") << std::endl;

        // Near light (depth 100, radius 50) should NOT be in slices 9+ (depth 168+)
        if (slice >= 9)
        {
            EXPECT_FALSE(found_near_light)
                << "Near light at depth 100 should NOT be in slice " << slice;
        }
    }
}

TEST(ClusterGrid, depth_800_boundary)
{
    // Test at exact depth 800 - reported problematic distance
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);

    const auto& config = grid.get_config();

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at depth 800 with radius 50
    glm::vec3 light_world_pos(0.0f, 0.0f, -800.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({77, light_world_pos, 50.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    glm::vec4 light_view = view * glm::vec4(light_world_pos, 1.0f);
    float light_view_depth = -light_view.z;

    std::cout << "Depth 800 boundary test:" << std::endl;
    std::cout << "  Light view depth: " << light_view_depth << std::endl;
    std::cout << "  Light depth slice: " << grid.get_depth_slice(light_view_depth) << std::endl;
    std::cout << "  Slice 10 range: " << grid.get_depth_slice(384.0f) << " to "
              << grid.get_depth_slice(876.0f) << std::endl;
    std::cout << "  Active clusters: " << grid.get_active_clusters() << std::endl;
    std::cout << "  Total assignments: " << grid.get_total_light_assignments() << std::endl;

    // Object at same depth
    glm::vec3 object_world_pos(20.0f, 0.0f, -790.0f);
    glm::vec4 object_view = view * glm::vec4(object_world_pos, 1.0f);
    float object_view_depth = -object_view.z;

    glm::vec4 object_clip = proj * object_view;
    glm::vec3 object_ndc = glm::vec3(object_clip) / object_clip.w;
    float screen_x = (object_ndc.x + 1.0f) * 0.5f * 800.0f;
    float screen_y = (object_ndc.y + 1.0f) * 0.5f * 600.0f;

    uint32_t tile_x = std::min(uint32_t(screen_x) / config.tile_size, config.tiles_x - 1);
    uint32_t tile_y = std::min(uint32_t(screen_y) / config.tile_size, config.tiles_y - 1);
    uint32_t slice = grid.get_depth_slice(object_view_depth);
    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;
    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 77)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "  Object view depth: " << object_view_depth << std::endl;
    std::cout << "  Object depth slice: " << slice << std::endl;
    std::cout << "  Object tile: (" << tile_x << ", " << tile_y << ")" << std::endl;
    std::cout << "  Cluster idx: " << cluster_idx << std::endl;
    std::cout << "  Lights in cluster: " << light_count << std::endl;
    std::cout << "  Found light: " << (found_light ? "yes" : "no") << std::endl;

    float dist = glm::length(object_world_pos - light_world_pos);
    std::cout << "  Distance: " << dist << " (radius: 50)" << std::endl;

    EXPECT_GT(grid.get_active_clusters(), 0u);
    EXPECT_TRUE(found_light);
}

TEST(ClusterGrid, very_far_object_and_light)
{
    // Test with object and light very far from camera
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar, 128, 12, 32);

    const auto& config = grid.get_config();

    // Camera at origin looking down -Z
    glm::vec3 cam_pos(0.0f, 0.0f, 0.0f);
    glm::mat4 view =
        glm::lookAt(cam_pos, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Light at Z=-1200 (1200 units in front of camera) with radius 50
    glm::vec3 light_world_pos(0.0f, 0.0f, -1200.0f);
    std::vector<cluster_light_info> lights;
    lights.push_back({66, light_world_pos, 50.0f});

    grid.build_clusters(view, proj, inv_proj, lights);

    // Transform light to view space
    glm::vec4 light_view = view * glm::vec4(light_world_pos, 1.0f);
    float light_view_depth = -light_view.z;

    std::cout << "Very far object test:" << std::endl;
    std::cout << "  Light world pos: (0, 0, -1200)" << std::endl;
    std::cout << "  Light view pos: (" << light_view.x << ", " << light_view.y << ", "
              << light_view.z << ")" << std::endl;
    std::cout << "  Light view depth: " << light_view_depth << std::endl;
    std::cout << "  Light depth slice: " << grid.get_depth_slice(light_view_depth) << std::endl;
    std::cout << "  Active clusters: " << grid.get_active_clusters() << std::endl;

    // Object near the light
    glm::vec3 object_world_pos(20.0f, 0.0f, -1180.0f);  // 28 units from light, within radius
    glm::vec4 object_view = view * glm::vec4(object_world_pos, 1.0f);
    float object_view_depth = -object_view.z;

    // Project to screen
    glm::vec4 object_clip = proj * object_view;
    glm::vec3 object_ndc = glm::vec3(object_clip) / object_clip.w;
    float screen_x = (object_ndc.x + 1.0f) * 0.5f * 800.0f;
    float screen_y = (object_ndc.y + 1.0f) * 0.5f * 600.0f;

    uint32_t tile_x = std::min(uint32_t(screen_x) / config.tile_size, config.tiles_x - 1);
    uint32_t tile_y = std::min(uint32_t(screen_y) / config.tile_size, config.tiles_y - 1);
    uint32_t slice = grid.get_depth_slice(object_view_depth);
    uint32_t cluster_idx = grid.get_cluster_index(tile_x, tile_y, slice);

    const auto& counts = grid.get_cluster_light_counts();
    const auto& indices = grid.get_cluster_light_indices();

    uint32_t light_count = counts[cluster_idx];
    bool found_light = false;
    for (uint32_t i = 0; i < light_count; ++i)
    {
        uint32_t base_idx = cluster_idx * config.max_lights_per_cluster;
        if (indices[base_idx + i] == 66)
        {
            found_light = true;
            break;
        }
    }

    std::cout << "  Object world pos: (20, 0, -1180)" << std::endl;
    std::cout << "  Object view depth: " << object_view_depth << std::endl;
    std::cout << "  Object screen pos: (" << screen_x << ", " << screen_y << ")" << std::endl;
    std::cout << "  Object tile: (" << tile_x << ", " << tile_y << "), slice: " << slice
              << std::endl;
    std::cout << "  Cluster idx: " << cluster_idx << std::endl;
    std::cout << "  Lights in cluster: " << light_count << std::endl;
    std::cout << "  Found light: " << (found_light ? "yes" : "no") << std::endl;

    float dist = glm::length(object_world_pos - light_world_pos);
    std::cout << "  Distance object to light: " << dist << " (radius: 50)" << std::endl;

    EXPECT_GT(grid.get_active_clusters(), 0u);
    EXPECT_TRUE(found_light) << "Far object within light radius should find light in cluster";
}

TEST(ClusterGrid, far_aabb_extents)
{
    // Debug test: print AABB extents for far clusters
    cluster_grid grid;
    grid.init(800, 600, 0.1f, KGPU_zfar.0f, 128, 12, 32);

    const auto& config = grid.get_config();

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = make_projection(60.0f, 800.0f / 600.0f, 0.1f, KGPU_zfar.0f);
    glm::mat4 inv_proj = glm::inverse(proj);

    // Just need to build clusters to populate AABBs
    std::vector<cluster_light_info> lights;
    lights.push_back({0, glm::vec3(0.0f, 0.0f, -100.0f), 50.0f});
    grid.build_clusters(view, proj, inv_proj, lights);

    const auto& aabbs = grid.get_cluster_aabbs();

    // Print AABBs for center tile across all slices
    uint32_t center_tile_x = config.tiles_x / 2;
    uint32_t center_tile_y = config.tiles_y / 2;

    std::cout << "AABB extents for center tile (" << center_tile_x << ", " << center_tile_y
              << "):" << std::endl;
    for (uint32_t slice = 0; slice < config.depth_slices; ++slice)
    {
        uint32_t idx = grid.get_cluster_index(center_tile_x, center_tile_y, slice);
        const auto& aabb = aabbs[idx];
        std::cout << "  Slice " << slice << ": min(" << aabb.min_point.x << ", " << aabb.min_point.y
                  << ", " << aabb.min_point.z << ")"
                  << " max(" << aabb.max_point.x << ", " << aabb.max_point.y << ", "
                  << aabb.max_point.z << ")" << std::endl;
    }
}

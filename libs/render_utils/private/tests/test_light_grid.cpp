#include <gtest/gtest.h>

#include "render_utils/light_grid.h"

using namespace agea::render;

// ============================================================================
// calc_light_radius tests
// ============================================================================

TEST(CalcLightRadius, quadratic_falloff)
{
    // Standard quadratic falloff: constant=1, linear=0.09, quadratic=0.032
    float radius = calc_light_radius(1.0f, 0.09f, 0.032f, 0.02f, 1.0f);

    // At threshold 0.02, attenuation = 50 (1/0.02)
    // 1 + 0.09*d + 0.032*d^2 = 50
    // 0.032*d^2 + 0.09*d - 49 = 0
    // d = (-0.09 + sqrt(0.0081 + 6.272)) / 0.064 = ~37.7
    EXPECT_GT(radius, 35.0f);
    EXPECT_LT(radius, 42.0f);
}

TEST(CalcLightRadius, linear_falloff_only)
{
    // Linear only: constant=1, linear=0.5, quadratic=0
    float radius = calc_light_radius(1.0f, 0.5f, 0.0f, 0.02f, 1.0f);

    // 1 + 0.5*d = 50 => d = 98
    EXPECT_NEAR(radius, 98.0f, 1.0f);
}

TEST(CalcLightRadius, no_falloff_returns_max)
{
    // No falloff: constant=1, linear=0, quadratic=0
    float radius = calc_light_radius(1.0f, 0.0f, 0.0f, 0.02f, 1.0f);

    EXPECT_EQ(radius, 1000.0f);
}

TEST(CalcLightRadius, margin_applied)
{
    float radius_no_margin = calc_light_radius(1.0f, 0.09f, 0.032f, 0.02f, 1.0f);
    float radius_with_margin = calc_light_radius(1.0f, 0.09f, 0.032f, 0.02f, 1.2f);

    EXPECT_NEAR(radius_with_margin, radius_no_margin * 1.2f, 0.1f);
}

// ============================================================================
// light_grid initialization tests
// ============================================================================

TEST(LightGrid, not_initialized_by_default)
{
    light_grid grid;
    EXPECT_FALSE(grid.is_initialized());
}

TEST(LightGrid, initialized_after_init)
{
    light_grid grid;
    grid.init(10.0f);
    EXPECT_TRUE(grid.is_initialized());
}

TEST(LightGrid, query_returns_zero_when_not_initialized)
{
    light_grid grid;
    uint32_t slots[10];

    uint32_t count = grid.query_lights(glm::vec3(0), 5.0f, slots, 10);
    EXPECT_EQ(count, 0u);
}

// ============================================================================
// Light insertion and query tests
// ============================================================================

TEST(LightGrid, insert_and_query_single_light)
{
    light_grid grid;
    grid.init(10.0f);

    // Insert light at origin with radius 15
    grid.insert_light(42, glm::vec3(0, 0, 0), 15.0f);

    uint32_t slots[10];

    // Query at origin should find the light
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 1.0f, slots, 10);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(slots[0], 42u);
}

TEST(LightGrid, query_misses_distant_light)
{
    light_grid grid;
    grid.init(10.0f);

    // Light at (50,0,0) with radius 10
    grid.insert_light(1, glm::vec3(50, 0, 0), 10.0f);

    uint32_t slots[10];

    // Query at (-50,0,0) with radius 5 - too far away
    uint32_t count = grid.query_lights(glm::vec3(-50, 0, 0), 5.0f, slots, 10);
    EXPECT_EQ(count, 0u);
}

TEST(LightGrid, query_finds_overlapping_light)
{
    light_grid grid;
    grid.init(10.0f);

    // Light at (20,0,0) with radius 15
    grid.insert_light(7, glm::vec3(20, 0, 0), 15.0f);

    uint32_t slots[10];

    // Query at (0,0,0) with radius 10 - spheres overlap (distance=20, sum of radii=25)
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 10.0f, slots, 10);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(slots[0], 7u);
}

TEST(LightGrid, multiple_lights_query)
{
    light_grid grid;
    grid.init(10.0f);

    // Insert lights near origin
    grid.insert_light(1, glm::vec3(5, 0, 0), 10.0f);
    grid.insert_light(2, glm::vec3(0, 5, 0), 10.0f);
    grid.insert_light(3, glm::vec3(0, 0, 5), 10.0f);

    // Insert one far away
    grid.insert_light(4, glm::vec3(80, 80, 80), 5.0f);

    uint32_t slots[16];
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 5.0f, slots, 16);

    // Should find the 3 nearby lights, not the far one
    EXPECT_EQ(count, 3u);
}

TEST(LightGrid, max_count_limits_results)
{
    light_grid grid;
    grid.init(10.0f);

    // Insert 5 lights at origin
    for (uint32_t i = 0; i < 5; ++i)
    {
        grid.insert_light(i, glm::vec3(0, 0, 0), 10.0f);
    }

    uint32_t slots[3];
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 1.0f, slots, 3);

    // Should only return 3 even though 5 are available
    EXPECT_EQ(count, 3u);
}

TEST(LightGrid, clear_removes_all_lights)
{
    light_grid grid;
    grid.init(10.0f);

    grid.insert_light(1, glm::vec3(0, 0, 0), 10.0f);
    grid.insert_light(2, glm::vec3(0, 0, 0), 10.0f);

    grid.clear();

    uint32_t slots[10];
    EXPECT_EQ(grid.query_lights(glm::vec3(0, 0, 0), 10.0f, slots, 10), 0u);
}

TEST(LightGrid, no_duplicates_in_query)
{
    light_grid grid;
    // Small cells to force light spanning multiple cells
    grid.init(5.0f);

    // Insert light with radius large enough to span multiple cells
    grid.insert_light(42, glm::vec3(0, 0, 0), 20.0f);

    uint32_t slots[10];
    // Query with large radius that also spans multiple cells
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 15.0f, slots, 10);

    // Should only return the light once, not once per cell
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(slots[0], 42u);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(LightGrid, negative_coordinates)
{
    light_grid grid;
    grid.init(10.0f);

    // Light in negative coordinate space
    grid.insert_light(1, glm::vec3(-50, -50, -50), 10.0f);

    uint32_t slots[10];
    uint32_t count = grid.query_lights(glm::vec3(-50, -50, -50), 5.0f, slots, 10);

    EXPECT_EQ(count, 1u);
}

TEST(LightGrid, zero_radius_query)
{
    light_grid grid;
    grid.init(10.0f);

    grid.insert_light(1, glm::vec3(0, 0, 0), 10.0f);

    uint32_t slots[10];
    // Query with zero radius at light position
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 0.0f, slots, 10);

    // distance=0 <= light_radius + query_radius = 10 + 0, so should be found
    EXPECT_EQ(count, 1u);
}

TEST(LightGrid, different_cell_sizes)
{
    // Test with very small cells
    light_grid grid_small;
    grid_small.init(1.0f);
    grid_small.insert_light(1, glm::vec3(0, 0, 0), 5.0f);

    uint32_t slots[10];
    EXPECT_EQ(grid_small.query_lights(glm::vec3(0, 0, 0), 1.0f, slots, 10), 1u);

    // Test with very large cells
    light_grid grid_large;
    grid_large.init(200.0f);
    grid_large.insert_light(1, glm::vec3(0, 0, 0), 5.0f);

    EXPECT_EQ(grid_large.query_lights(glm::vec3(0, 0, 0), 1.0f, slots, 10), 1u);
}

TEST(LightGrid, sparse_world_memory_efficiency)
{
    light_grid grid;
    grid.init(10.0f);

    // Insert lights at very distant locations (would require huge array with vector approach)
    grid.insert_light(1, glm::vec3(1000, 0, 0), 5.0f);
    grid.insert_light(2, glm::vec3(-1000, 0, 0), 5.0f);
    grid.insert_light(3, glm::vec3(0, 1000, 0), 5.0f);

    uint32_t slots[10];

    // Query near first light
    uint32_t count1 = grid.query_lights(glm::vec3(1000, 0, 0), 5.0f, slots, 10);
    EXPECT_EQ(count1, 1u);
    EXPECT_EQ(slots[0], 1u);

    // Query near second light
    uint32_t count2 = grid.query_lights(glm::vec3(-1000, 0, 0), 5.0f, slots, 10);
    EXPECT_EQ(count2, 1u);
    EXPECT_EQ(slots[0], 2u);

    // Query at origin finds nothing
    uint32_t count3 = grid.query_lights(glm::vec3(0, 0, 0), 5.0f, slots, 10);
    EXPECT_EQ(count3, 0u);
}

TEST(LightGrid, reinit_clears_existing_data)
{
    light_grid grid;
    grid.init(10.0f);

    grid.insert_light(1, glm::vec3(0, 0, 0), 10.0f);

    // Reinitialize
    grid.init(20.0f);

    uint32_t slots[10];
    uint32_t count = grid.query_lights(glm::vec3(0, 0, 0), 5.0f, slots, 10);

    // Old data should be cleared
    EXPECT_EQ(count, 0u);
}

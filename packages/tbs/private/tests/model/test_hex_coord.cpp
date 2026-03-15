#include <gtest/gtest.h>

#include "packages/tbs/model/hex_coord.h"

#include <unordered_set>
#include <algorithm>

using namespace kryga::tbs;

class HexCoordTest : public ::testing::Test
{
protected:
    static constexpr float HEX_SIZE = 1.0f;
    static constexpr float EPSILON = 0.0001f;
};

// Basic construction and equality
TEST_F(HexCoordTest, default_construction)
{
    hex_coord h;
    EXPECT_EQ(h.q, 0);
    EXPECT_EQ(h.r, 0);
}

TEST_F(HexCoordTest, value_construction)
{
    hex_coord h(3, -2);
    EXPECT_EQ(h.q, 3);
    EXPECT_EQ(h.r, -2);
}

TEST_F(HexCoordTest, cube_coordinate_s)
{
    // s = -q - r, so q + r + s = 0
    hex_coord h(3, -2);
    EXPECT_EQ(h.s(), -1);
    EXPECT_EQ(h.q + h.r + h.s(), 0);

    hex_coord h2(-5, 2);
    EXPECT_EQ(h2.s(), 3);
    EXPECT_EQ(h2.q + h2.r + h2.s(), 0);
}

TEST_F(HexCoordTest, equality)
{
    hex_coord a(1, 2);
    hex_coord b(1, 2);
    hex_coord c(2, 1);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// Arithmetic operations
TEST_F(HexCoordTest, addition)
{
    hex_coord a(1, 2);
    hex_coord b(3, -1);
    hex_coord c = a + b;

    EXPECT_EQ(c.q, 4);
    EXPECT_EQ(c.r, 1);
}

TEST_F(HexCoordTest, subtraction)
{
    hex_coord a(5, 3);
    hex_coord b(2, 1);
    hex_coord c = a - b;

    EXPECT_EQ(c.q, 3);
    EXPECT_EQ(c.r, 2);
}

TEST_F(HexCoordTest, scalar_multiplication)
{
    hex_coord a(2, -3);
    hex_coord b = a * 3;

    EXPECT_EQ(b.q, 6);
    EXPECT_EQ(b.r, -9);
}

TEST_F(HexCoordTest, compound_assignment)
{
    hex_coord a(1, 1);
    hex_coord b(2, 3);

    a += b;
    EXPECT_EQ(a.q, 3);
    EXPECT_EQ(a.r, 4);

    a -= b;
    EXPECT_EQ(a.q, 1);
    EXPECT_EQ(a.r, 1);
}

// Direction and neighbor tests
TEST_F(HexCoordTest, directions_count)
{
    auto dirs = hex_coord::directions();
    EXPECT_EQ(dirs.size(), 6u);
}

TEST_F(HexCoordTest, neighbor_single)
{
    hex_coord center(0, 0);

    // E neighbor
    hex_coord e = center.neighbor(0);
    EXPECT_EQ(e, hex_coord(1, 0));

    // NE neighbor
    hex_coord ne = center.neighbor(1);
    EXPECT_EQ(ne, hex_coord(1, -1));

    // W neighbor
    hex_coord w = center.neighbor(3);
    EXPECT_EQ(w, hex_coord(-1, 0));
}

TEST_F(HexCoordTest, all_neighbors)
{
    hex_coord center(2, 3);
    auto neighs = center.neighbors();

    EXPECT_EQ(neighs.size(), 6u);

    // Verify all neighbors are at distance 1
    for (const auto& n : neighs)
    {
        EXPECT_EQ(center.distance_to(n), 1);
    }

    // Verify no duplicates
    std::unordered_set<hex_coord, hex_coord_hash> unique_neighs(neighs.begin(), neighs.end());
    EXPECT_EQ(unique_neighs.size(), 6u);
}

TEST_F(HexCoordTest, neighbor_wraps_direction)
{
    hex_coord center(0, 0);
    // Direction 6 should wrap to direction 0
    EXPECT_EQ(center.neighbor(6), center.neighbor(0));
    EXPECT_EQ(center.neighbor(12), center.neighbor(0));
}

// Distance tests
TEST_F(HexCoordTest, distance_to_self)
{
    hex_coord a(3, -2);
    EXPECT_EQ(a.distance_to(a), 0);
}

TEST_F(HexCoordTest, distance_to_neighbor)
{
    hex_coord a(0, 0);
    for (const auto& n : a.neighbors())
    {
        EXPECT_EQ(a.distance_to(n), 1);
    }
}

TEST_F(HexCoordTest, distance_symmetric)
{
    hex_coord a(3, -2);
    hex_coord b(-1, 5);

    EXPECT_EQ(a.distance_to(b), b.distance_to(a));
}

TEST_F(HexCoordTest, distance_straight_line)
{
    hex_coord origin(0, 0);
    hex_coord far_east(5, 0);

    EXPECT_EQ(origin.distance_to(far_east), 5);
}

TEST_F(HexCoordTest, distance_diagonal)
{
    hex_coord origin(0, 0);
    hex_coord diagonal(3, 3);  // SE direction

    EXPECT_EQ(origin.distance_to(diagonal), 6);
}

// World coordinate conversion tests
TEST_F(HexCoordTest, origin_to_world)
{
    hex_coord origin(0, 0);
    glm::vec3 world = origin.to_world(HEX_SIZE);

    EXPECT_NEAR(world.x, 0.0f, EPSILON);
    EXPECT_NEAR(world.y, 0.0f, EPSILON);
    EXPECT_NEAR(world.z, 0.0f, EPSILON);
}

TEST_F(HexCoordTest, to_world_and_back)
{
    // Test round-trip conversion
    std::vector<hex_coord> test_coords = {{0, 0},  {1, 0},  {0, 1},  {-1, 1},
                                          {2, -1}, {-3, 5}, {10, -7}};

    for (const auto& original : test_coords)
    {
        glm::vec3 world = original.to_world(HEX_SIZE);
        hex_coord recovered = hex_coord::from_world(world, HEX_SIZE);
        EXPECT_EQ(original, recovered) << "Failed for (" << original.q << ", " << original.r << ")";
    }
}

TEST_F(HexCoordTest, from_world_rounds_correctly)
{
    hex_coord center(2, 3);
    glm::vec3 world_center = center.to_world(HEX_SIZE);

    // Points slightly offset from center should still round to same hex
    glm::vec3 offset(0.1f, 0.0f, 0.1f);
    hex_coord recovered = hex_coord::from_world(world_center + offset, HEX_SIZE);
    EXPECT_EQ(center, recovered);
}

TEST_F(HexCoordTest, to_world_different_sizes)
{
    hex_coord h(1, 0);

    glm::vec3 world1 = h.to_world(1.0f);
    glm::vec3 world2 = h.to_world(2.0f);

    // Doubling hex size should double world coordinates
    EXPECT_NEAR(world2.x, world1.x * 2.0f, EPSILON);
    EXPECT_NEAR(world2.z, world1.z * 2.0f, EPSILON);
}

// Range query tests
TEST_F(HexCoordTest, hexes_in_range_zero)
{
    hex_coord center(3, -2);
    auto hexes = hex_coord::hexes_in_range(center, 0);

    EXPECT_EQ(hexes.size(), 1u);
    EXPECT_EQ(hexes[0], center);
}

TEST_F(HexCoordTest, hexes_in_range_one)
{
    hex_coord center(0, 0);
    auto hexes = hex_coord::hexes_in_range(center, 1);

    // Range 1 = center + 6 neighbors = 7
    EXPECT_EQ(hexes.size(), 7u);

    // All should be within distance 1
    for (const auto& h : hexes)
    {
        EXPECT_LE(center.distance_to(h), 1);
    }
}

TEST_F(HexCoordTest, hexes_in_range_formula)
{
    // Number of hexes in range r = 3*r*(r+1) + 1
    hex_coord center(0, 0);

    for (int r = 0; r <= 5; ++r)
    {
        auto hexes = hex_coord::hexes_in_range(center, r);
        size_t expected = static_cast<size_t>(3 * r * (r + 1) + 1);
        EXPECT_EQ(hexes.size(), expected) << "Failed for radius " << r;
    }
}

TEST_F(HexCoordTest, hexes_in_range_no_duplicates)
{
    hex_coord center(5, -3);
    auto hexes = hex_coord::hexes_in_range(center, 3);

    std::unordered_set<hex_coord, hex_coord_hash> unique(hexes.begin(), hexes.end());
    EXPECT_EQ(unique.size(), hexes.size());
}

// Ring tests
TEST_F(HexCoordTest, hex_ring_zero)
{
    hex_coord center(1, 2);
    auto ring = hex_coord::hex_ring(center, 0);

    EXPECT_EQ(ring.size(), 1u);
    EXPECT_EQ(ring[0], center);
}

TEST_F(HexCoordTest, hex_ring_one)
{
    hex_coord center(0, 0);
    auto ring = hex_coord::hex_ring(center, 1);

    // Ring at distance 1 = 6 hexes
    EXPECT_EQ(ring.size(), 6u);

    // All should be at exactly distance 1
    for (const auto& h : ring)
    {
        EXPECT_EQ(center.distance_to(h), 1);
    }
}

TEST_F(HexCoordTest, hex_ring_formula)
{
    // Ring at radius r has 6*r hexes (except r=0 which has 1)
    hex_coord center(0, 0);

    for (int r = 1; r <= 5; ++r)
    {
        auto ring = hex_coord::hex_ring(center, r);
        EXPECT_EQ(ring.size(), static_cast<size_t>(6 * r)) << "Failed for radius " << r;
    }
}

TEST_F(HexCoordTest, hex_ring_all_at_correct_distance)
{
    hex_coord center(-2, 4);
    int radius = 3;
    auto ring = hex_coord::hex_ring(center, radius);

    for (const auto& h : ring)
    {
        EXPECT_EQ(center.distance_to(h), radius);
    }
}

// Line tests
TEST_F(HexCoordTest, hex_line_same_point)
{
    hex_coord a(3, 2);
    auto line = hex_coord::hex_line(a, a);

    EXPECT_EQ(line.size(), 1u);
    EXPECT_EQ(line[0], a);
}

TEST_F(HexCoordTest, hex_line_neighbors)
{
    hex_coord a(0, 0);
    hex_coord b(1, 0);
    auto line = hex_coord::hex_line(a, b);

    EXPECT_EQ(line.size(), 2u);
    EXPECT_EQ(line[0], a);
    EXPECT_EQ(line[1], b);
}

TEST_F(HexCoordTest, hex_line_length)
{
    hex_coord a(0, 0);
    hex_coord b(5, -2);

    int distance = a.distance_to(b);
    auto line = hex_coord::hex_line(a, b);

    // Line should have distance + 1 hexes
    EXPECT_EQ(line.size(), static_cast<size_t>(distance + 1));
}

TEST_F(HexCoordTest, hex_line_endpoints)
{
    hex_coord a(-3, 2);
    hex_coord b(4, -1);
    auto line = hex_coord::hex_line(a, b);

    EXPECT_EQ(line.front(), a);
    EXPECT_EQ(line.back(), b);
}

TEST_F(HexCoordTest, hex_line_consecutive_neighbors)
{
    hex_coord a(0, 0);
    hex_coord b(3, 2);
    auto line = hex_coord::hex_line(a, b);

    // Each hex should be neighbor of the previous
    for (size_t i = 1; i < line.size(); ++i)
    {
        EXPECT_EQ(line[i - 1].distance_to(line[i]), 1)
            << "Gap at index " << i << ": (" << line[i - 1].q << "," << line[i - 1].r << ") to ("
            << line[i].q << "," << line[i].r << ")";
    }
}

// Hash tests
TEST_F(HexCoordTest, hash_different_for_different_coords)
{
    hex_coord_hash hasher;

    hex_coord a(1, 2);
    hex_coord b(2, 1);
    hex_coord c(1, 3);

    // Different coords should (usually) have different hashes
    // Note: hash collisions are possible but unlikely for small values
    EXPECT_NE(hasher(a), hasher(b));
    EXPECT_NE(hasher(a), hasher(c));
}

TEST_F(HexCoordTest, hash_same_for_same_coords)
{
    hex_coord_hash hasher;

    hex_coord a(5, -3);
    hex_coord b(5, -3);

    EXPECT_EQ(hasher(a), hasher(b));
}

TEST_F(HexCoordTest, usable_in_unordered_set)
{
    std::unordered_set<hex_coord, hex_coord_hash> set;

    set.insert({0, 0});
    set.insert({1, 0});
    set.insert({0, 1});
    set.insert({0, 0});  // Duplicate

    EXPECT_EQ(set.size(), 3u);
    EXPECT_TRUE(set.count({0, 0}) == 1);
    EXPECT_TRUE(set.count({2, 2}) == 0);
}

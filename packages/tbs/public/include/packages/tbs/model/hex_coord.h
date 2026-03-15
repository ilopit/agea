#pragma once

#include <array>
#include <cmath>
#include <vector>
#include <functional>

#include <glm/glm.hpp>

namespace kryga::tbs
{

// Axial hex coordinate system (q = column, r = row)
// Uses "pointy-top" hex orientation
//
// Coordinate layout (pointy-top, +X right, +Z down):
//
//         ___        ___
//        /   \      /   \
//       / 0,0 \____/ 1,0 \
//       \     /    \     /
//        \___/ 0,1  \___/
//        /   \      /   \
//       /     \____/ 1,1 \
//       \     /    \     /
//        \___/      \___/
//
// Directions (clockwise from E):
//             __
//         NW /  \ NE
//       W   <    >   E
//         SW \__/ SE
//
struct hex_coord
{
    int q = 0;  // column
    int r = 0;  // row

    constexpr hex_coord() = default;
    constexpr hex_coord(int q_, int r_)
        : q(q_)
        , r(r_)
    {
    }

    // Cube coordinate s (x + y + z = 0 constraint, where x=q, z=r, y=s)
    constexpr int
    s() const
    {
        return -q - r;
    }

    // Equality
    constexpr bool
    operator==(const hex_coord& other) const
    {
        return q == other.q && r == other.r;
    }
    constexpr bool
    operator!=(const hex_coord& other) const
    {
        return !(*this == other);
    }

    // Arithmetic
    constexpr hex_coord
    operator+(const hex_coord& other) const
    {
        return hex_coord(q + other.q, r + other.r);
    }
    constexpr hex_coord
    operator-(const hex_coord& other) const
    {
        return hex_coord(q - other.q, r - other.r);
    }
    constexpr hex_coord
    operator*(int scalar) const
    {
        return hex_coord(q * scalar, r * scalar);
    }

    constexpr hex_coord&
    operator+=(const hex_coord& other)
    {
        q += other.q;
        r += other.r;
        return *this;
    }
    constexpr hex_coord&
    operator-=(const hex_coord& other)
    {
        q -= other.q;
        r -= other.r;
        return *this;
    }

    // Direction vectors for the 6 neighbors (pointy-top orientation)
    // Order: E, NE, NW, W, SW, SE (clockwise from east)
    static constexpr std::array<hex_coord, 6>
    directions()
    {
        return {{
            {1, 0},   // E
            {1, -1},  // NE
            {0, -1},  // NW
            {-1, 0},  // W
            {-1, 1},  // SW
            {0, 1}    // SE
        }};
    }

    // Get neighbor in given direction (0-5)
    constexpr hex_coord
    neighbor(int direction) const
    {
        const auto& dirs = directions();
        return *this + dirs[direction % 6];
    }

    // Get all 6 neighbors
    std::array<hex_coord, 6>
    neighbors() const;

    // Manhattan distance on hex grid (minimum steps to reach other)
    int
    distance_to(const hex_coord& other) const;

    // Convert to world space position (XZ plane, Y=0)
    // hex_size is the distance from center to corner
    glm::vec3
    to_world(float hex_size) const;

    // Convert world position to hex coordinate (rounds to nearest hex)
    static hex_coord
    from_world(const glm::vec3& world_pos, float hex_size);

    // Get all hexes within given radius (inclusive)
    static std::vector<hex_coord>
    hexes_in_range(const hex_coord& center, int radius);

    // Get ring of hexes at exactly given radius
    static std::vector<hex_coord>
    hex_ring(const hex_coord& center, int radius);

    // Line drawing - get all hexes on line from start to end
    static std::vector<hex_coord>
    hex_line(const hex_coord& start, const hex_coord& end);
};

// For use in std::unordered_map/set
struct hex_coord_hash
{
    std::size_t
    operator()(const hex_coord& coord) const
    {
        // Combine q and r into single hash
        // Using a large prime to spread values
        return std::hash<int>()(coord.q) ^ (std::hash<int>()(coord.r) << 16);
    }
};

}  // namespace kryga::tbs

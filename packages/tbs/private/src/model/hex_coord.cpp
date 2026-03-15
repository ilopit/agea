#include "packages/tbs/model/hex_coord.h"

#include <algorithm>
#include <cmath>

namespace kryga::tbs
{

std::array<hex_coord, 6>
hex_coord::neighbors() const
{
    const auto& dirs = directions();
    return {{
        *this + dirs[0],
        *this + dirs[1],
        *this + dirs[2],
        *this + dirs[3],
        *this + dirs[4],
        *this + dirs[5],
    }};
}

int
hex_coord::distance_to(const hex_coord& other) const
{
    // In cube coordinates, distance = (|dx| + |dy| + |dz|) / 2
    // Since s = -q - r, we have:
    // dx = other.q - q
    // dy = other.s() - s() = (-other.q - other.r) - (-q - r) = (q - other.q) + (r - other.r)
    // dz = other.r - r
    const int dq = other.q - q;
    const int dr = other.r - r;
    const int ds = other.s() - s();

    return (std::abs(dq) + std::abs(dr) + std::abs(ds)) / 2;
}

glm::vec3
hex_coord::to_world(float hex_size) const
{
    // Pointy-top hex layout
    // x = size * sqrt(3) * (q + r/2)
    // z = size * 3/2 * r
    const float sqrt3 = 1.7320508075688772f;

    const float x = hex_size * sqrt3 * (static_cast<float>(q) + static_cast<float>(r) / 2.0f);
    const float z = hex_size * 1.5f * static_cast<float>(r);

    return glm::vec3(x, 0.0f, z);
}

hex_coord
hex_coord::from_world(const glm::vec3& world_pos, float hex_size)
{
    // Inverse of to_world, then round to nearest hex
    const float sqrt3 = 1.7320508075688772f;

    // Convert to fractional axial coordinates
    const float fq = (sqrt3 / 3.0f * world_pos.x - 1.0f / 3.0f * world_pos.z) / hex_size;
    const float fr = (2.0f / 3.0f * world_pos.z) / hex_size;

    // Convert to cube coordinates for rounding
    const float fx = fq;
    const float fz = fr;
    const float fy = -fx - fz;

    // Round to nearest integer cube coordinates
    int rx = static_cast<int>(std::round(fx));
    int ry = static_cast<int>(std::round(fy));
    int rz = static_cast<int>(std::round(fz));

    // Fix rounding errors - cube coordinates must sum to 0
    const float dx = std::abs(static_cast<float>(rx) - fx);
    const float dy = std::abs(static_cast<float>(ry) - fy);
    const float dz = std::abs(static_cast<float>(rz) - fz);

    if (dx > dy && dx > dz)
    {
        rx = -ry - rz;
    }
    else if (dy > dz)
    {
        ry = -rx - rz;
    }
    else
    {
        rz = -rx - ry;
    }

    // Convert back to axial (q = x, r = z)
    return hex_coord(rx, rz);
}

std::vector<hex_coord>
hex_coord::hexes_in_range(const hex_coord& center, int radius)
{
    std::vector<hex_coord> results;
    results.reserve(static_cast<size_t>(3 * radius * (radius + 1) + 1));  // Hex count formula

    for (int dq = -radius; dq <= radius; ++dq)
    {
        const int r_min = std::max(-radius, -dq - radius);
        const int r_max = std::min(radius, -dq + radius);
        for (int dr = r_min; dr <= r_max; ++dr)
        {
            results.emplace_back(center.q + dq, center.r + dr);
        }
    }

    return results;
}

std::vector<hex_coord>
hex_coord::hex_ring(const hex_coord& center, int radius)
{
    if (radius == 0)
    {
        return {center};
    }

    std::vector<hex_coord> results;
    results.reserve(static_cast<size_t>(6 * radius));

    // Start at one corner and walk around
    const auto& dirs = directions();
    hex_coord current = center + dirs[4] * radius;  // Start at SW corner

    for (int i = 0; i < 6; ++i)
    {
        for (int j = 0; j < radius; ++j)
        {
            results.push_back(current);
            current = current + dirs[i];
        }
    }

    return results;
}

std::vector<hex_coord>
hex_coord::hex_line(const hex_coord& start, const hex_coord& end)
{
    const int n = start.distance_to(end);
    if (n == 0)
    {
        return {start};
    }

    std::vector<hex_coord> results;
    results.reserve(static_cast<size_t>(n + 1));

    // Linear interpolation in cube space, then round
    const float sqrt3 = 1.7320508075688772f;
    const float hex_size = 1.0f;  // Using unit size for interpolation

    const glm::vec3 start_world = start.to_world(hex_size);
    const glm::vec3 end_world = end.to_world(hex_size);

    for (int i = 0; i <= n; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const glm::vec3 lerped = start_world + t * (end_world - start_world);
        results.push_back(from_world(lerped, hex_size));
    }

    return results;
}

}  // namespace kryga::tbs

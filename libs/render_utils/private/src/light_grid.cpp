#include "render_utils/light_grid.h"

#include <algorithm>

namespace agea
{
namespace render
{

void
light_grid::init(float cell_size)
{
    m_cell_size = cell_size;
    m_inv_cell_size = 1.0f / cell_size;
    m_cells.clear();
    m_initialized = true;
}

void
light_grid::clear()
{
    m_cells.clear();
}

glm::ivec3
light_grid::world_to_cell(const glm::vec3& pos) const
{
    return glm::ivec3(static_cast<int>(std::floor(pos.x * m_inv_cell_size)),
                      static_cast<int>(std::floor(pos.y * m_inv_cell_size)),
                      static_cast<int>(std::floor(pos.z * m_inv_cell_size)));
}

uint64_t
light_grid::make_cell_key(int x, int y, int z)
{
    // Pack 3 signed 21-bit integers into 64 bits
    // This supports coordinates from -1048576 to 1048575
    constexpr uint64_t MASK_21 = 0x1FFFFFULL;
    return (static_cast<uint64_t>(x) & MASK_21) | ((static_cast<uint64_t>(y) & MASK_21) << 21) |
           ((static_cast<uint64_t>(z) & MASK_21) << 42);
}

void
light_grid::insert_light(uint32_t slot, const glm::vec3& pos, float radius)
{
    if (!m_initialized)
        return;

    light_grid_entry entry{slot, radius, pos};

    // Calculate AABB of light sphere
    glm::vec3 light_min = pos - glm::vec3(radius);
    glm::vec3 light_max = pos + glm::vec3(radius);

    glm::ivec3 cell_min = world_to_cell(light_min);
    glm::ivec3 cell_max = world_to_cell(light_max);

    // Insert into all overlapping cells
    for (int z = cell_min.z; z <= cell_max.z; ++z)
    {
        for (int y = cell_min.y; y <= cell_max.y; ++y)
        {
            for (int x = cell_min.x; x <= cell_max.x; ++x)
            {
                m_cells[make_cell_key(x, y, z)].push_back(entry);
            }
        }
    }
}

uint32_t
light_grid::query_lights(const glm::vec3& center,
                         float radius,
                         uint32_t* out_slots,
                         uint32_t max_count) const
{
    if (!m_initialized || max_count == 0)
        return 0;

    // Calculate cells overlapped by query sphere
    glm::vec3 query_min = center - glm::vec3(radius);
    glm::vec3 query_max = center + glm::vec3(radius);

    glm::ivec3 cell_min = world_to_cell(query_min);
    glm::ivec3 cell_max = world_to_cell(query_max);

    uint32_t count = 0;

    for (int z = cell_min.z; z <= cell_max.z; ++z)
    {
        for (int y = cell_min.y; y <= cell_max.y; ++y)
        {
            for (int x = cell_min.x; x <= cell_max.x; ++x)
            {
                auto it = m_cells.find(make_cell_key(x, y, z));
                if (it == m_cells.end())
                    continue;

                for (const auto& entry : it->second)
                {
                    // Sphere-sphere intersection test
                    float dist = glm::length(entry.position - center);
                    if (dist <= entry.radius + radius)
                    {
                        // Check for duplicates
                        bool duplicate = false;
                        for (uint32_t i = 0; i < count; ++i)
                        {
                            if (out_slots[i] == entry.slot)
                            {
                                duplicate = true;
                                break;
                            }
                        }

                        if (!duplicate)
                        {
                            out_slots[count] = entry.slot;
                            ++count;
                            if (count >= max_count)
                                return count;
                        }
                    }
                }
            }
        }
    }

    return count;
}

}  // namespace render
}  // namespace agea

#pragma once

#include <glm_unofficial/glm.h>

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cmath>

namespace agea
{
namespace render
{

enum class light_type
{
    spot = 0,
    point
};

// Calculates light effective radius from attenuation parameters
// attenuation = 1 / (constant + linear*d + quadratic*d^2)
// Solves for d where attenuation = threshold, then adds margin
inline float
calc_light_radius(
    float constant, float linear, float quadratic, float threshold = 0.02f, float margin = 1.05f)
{
    // Solve: constant + linear*d + quadratic*d^2 = 1/threshold
    // quadratic*d^2 + linear*d + (constant - 1/threshold) = 0
    // Using quadratic formula: d = (-b + sqrt(b^2 - 4ac)) / 2a
    // where a = quadratic, b = linear, c = constant - 1/threshold

    const float inv_threshold = 1.0f / threshold;
    const float c = constant - inv_threshold;

    if (quadratic < 0.0001f)
    {
        // Linear falloff only
        if (linear < 0.0001f)
        {
            // No falloff, infinite radius - cap it
            return 1000.0f;
        }
        // linear*d + c = 0 => d = -c / linear
        float d = -c / linear;
        return d > 0.0f ? d * margin : 1000.0f;
    }

    const float discriminant = linear * linear - 4.0f * quadratic * c;
    if (discriminant < 0.0f)
    {
        // No real solution - light never gets bright enough
        return 0.0f;
    }

    const float d = (-linear + std::sqrt(discriminant)) / (2.0f * quadratic);
    return d > 0.0f ? d * margin : 0.0f;
}

struct light_grid_entry
{
    uint32_t slot;
    float radius;
    glm::vec3 position;
};

// Hash for cell coordinates packed into uint64_t
struct cell_key_hash
{
    size_t
    operator()(uint64_t key) const noexcept
    {
        // Mix bits for better distribution
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccdULL;
        key ^= key >> 33;
        key *= 0xc4ceb9fe1a85ec53ULL;
        key ^= key >> 33;
        return static_cast<size_t>(key);
    }
};

class light_grid
{
public:
    static constexpr float DEFAULT_CELL_SIZE = 10.0f;
    static constexpr uint32_t DEFAULT_MAX_LIGHTS = 16;

    light_grid() = default;

    // Initialize with cell size only (unbounded world)
    void
    init(float cell_size = DEFAULT_CELL_SIZE);

    void
    clear();

    // Unified light insertion
    void
    insert_light(uint32_t slot, const glm::vec3& pos, float radius);

    // Query all lights affecting a sphere. Returns count of lights written to out_slots.
    uint32_t
    query_lights(const glm::vec3& center,
                 float radius,
                 uint32_t* out_slots,
                 uint32_t max_count) const;

    bool
    is_initialized() const
    {
        return m_initialized;
    }

private:
    glm::ivec3
    world_to_cell(const glm::vec3& pos) const;

    // Pack cell coordinates into a single 64-bit key
    static uint64_t
    make_cell_key(int x, int y, int z);
    static uint64_t
    make_cell_key(const glm::ivec3& c)
    {
        return make_cell_key(c.x, c.y, c.z);
    }

    float m_cell_size = DEFAULT_CELL_SIZE;
    float m_inv_cell_size = 1.0f / DEFAULT_CELL_SIZE;

    std::unordered_map<uint64_t, std::vector<light_grid_entry>, cell_key_hash> m_cells;

    bool m_initialized = false;
};

}  // namespace render
}  // namespace agea

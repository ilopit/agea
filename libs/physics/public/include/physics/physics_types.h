#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace physics
{

struct chunk_shape
{
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
    glm::vec3 seed_point{0.0f};
    std::vector<glm::vec3> hull_points;
};

struct impact
{
    glm::vec3 point{0.0f};
    glm::vec3 impulse{0.0f};
    float damage = 0.0f;
};

struct destructible_handle
{
    uint64_t value = 0;

    bool
    valid() const
    {
        return value != 0;
    }
};

}  // namespace physics
}  // namespace kryga

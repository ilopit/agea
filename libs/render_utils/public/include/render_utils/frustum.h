#pragma once

#include <glm_unofficial/glm.h>

#include <array>

namespace agea
{
namespace render
{

// Plane equation: dot(normal, point) + distance = 0
// Points on positive side (dot + d > 0) are in front of plane
struct plane
{
    glm::vec3 normal;
    float distance;

    float
    distance_to_point(const glm::vec3& point) const
    {
        return glm::dot(normal, point) + distance;
    }
};

class frustum
{
public:
    enum plane_id
    {
        PLANE_LEFT = 0,
        PLANE_RIGHT,
        PLANE_BOTTOM,
        PLANE_TOP,
        PLANE_NEAR,
        PLANE_FAR,
        PLANE_COUNT
    };

    frustum() = default;

    // Extract frustum planes from view-projection matrix
    void
    extract_planes(const glm::mat4& view_projection);

    // Test if sphere is inside or intersecting frustum
    // Returns true if sphere should be rendered (visible)
    bool
    is_sphere_visible(const glm::vec3& center, float radius) const;

    const plane&
    get_plane(plane_id id) const
    {
        return m_planes[id];
    }

private:
    std::array<plane, PLANE_COUNT> m_planes;
};

}  // namespace render
}  // namespace agea

#include "render_utils/frustum.h"

namespace agea
{
namespace render
{

void
frustum::extract_planes(const glm::mat4& vp)
{
    // Extract frustum planes from view-projection matrix
    // Using Gribb/Hartmann method
    // Each plane: (A, B, C, D) where Ax + By + Cz + D = 0

    // Left plane: row3 + row0
    m_planes[PLANE_LEFT].normal.x = vp[0][3] + vp[0][0];
    m_planes[PLANE_LEFT].normal.y = vp[1][3] + vp[1][0];
    m_planes[PLANE_LEFT].normal.z = vp[2][3] + vp[2][0];
    m_planes[PLANE_LEFT].distance = vp[3][3] + vp[3][0];

    // Right plane: row3 - row0
    m_planes[PLANE_RIGHT].normal.x = vp[0][3] - vp[0][0];
    m_planes[PLANE_RIGHT].normal.y = vp[1][3] - vp[1][0];
    m_planes[PLANE_RIGHT].normal.z = vp[2][3] - vp[2][0];
    m_planes[PLANE_RIGHT].distance = vp[3][3] - vp[3][0];

    // Bottom plane: row3 + row1
    m_planes[PLANE_BOTTOM].normal.x = vp[0][3] + vp[0][1];
    m_planes[PLANE_BOTTOM].normal.y = vp[1][3] + vp[1][1];
    m_planes[PLANE_BOTTOM].normal.z = vp[2][3] + vp[2][1];
    m_planes[PLANE_BOTTOM].distance = vp[3][3] + vp[3][1];

    // Top plane: row3 - row1
    m_planes[PLANE_TOP].normal.x = vp[0][3] - vp[0][1];
    m_planes[PLANE_TOP].normal.y = vp[1][3] - vp[1][1];
    m_planes[PLANE_TOP].normal.z = vp[2][3] - vp[2][1];
    m_planes[PLANE_TOP].distance = vp[3][3] - vp[3][1];

    // Near plane: row3 + row2
    m_planes[PLANE_NEAR].normal.x = vp[0][3] + vp[0][2];
    m_planes[PLANE_NEAR].normal.y = vp[1][3] + vp[1][2];
    m_planes[PLANE_NEAR].normal.z = vp[2][3] + vp[2][2];
    m_planes[PLANE_NEAR].distance = vp[3][3] + vp[3][2];

    // Far plane: row3 - row2
    m_planes[PLANE_FAR].normal.x = vp[0][3] - vp[0][2];
    m_planes[PLANE_FAR].normal.y = vp[1][3] - vp[1][2];
    m_planes[PLANE_FAR].normal.z = vp[2][3] - vp[2][2];
    m_planes[PLANE_FAR].distance = vp[3][3] - vp[3][2];

    // Normalize all planes
    for (auto& p : m_planes)
    {
        float len = glm::length(p.normal);
        if (len > 0.0001f)
        {
            p.normal /= len;
            p.distance /= len;
        }
    }
}

bool
frustum::is_sphere_visible(const glm::vec3& center, float radius) const
{
    // Test sphere against all 6 planes
    // If sphere is completely behind any plane, it's outside frustum
    // Add small bias for floating-point precision at boundaries
    const float bias = 0.01f;
    float adjusted_radius = radius + bias;

    for (const auto& p : m_planes)
    {
        float dist = p.distance_to_point(center);
        if (dist < -adjusted_radius)
        {
            return false;  // Sphere is completely outside this plane
        }
    }
    return true;  // Sphere is inside or intersecting frustum
}

}  // namespace render
}  // namespace agea

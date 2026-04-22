#include "vfx/emitter.h"

#include <utils/check.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace kryga
{
namespace vfx
{

namespace
{

inline float
uniform(std::mt19937& rng, float lo, float hi)
{
    if (lo >= hi)
    {
        return lo;
    }
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}

inline glm::vec3
random_point_in_sphere(std::mt19937& rng, float radius)
{
    for (;;)
    {
        glm::vec3 p{uniform(rng, -1.0f, 1.0f), uniform(rng, -1.0f, 1.0f), uniform(rng, -1.0f, 1.0f)};
        if (glm::dot(p, p) <= 1.0f)
        {
            return p * radius;
        }
    }
}

inline glm::vec3
random_point_in_box(std::mt19937& rng, const glm::vec3& extents)
{
    return glm::vec3{uniform(rng, -extents.x, extents.x),
                     uniform(rng, -extents.y, extents.y),
                     uniform(rng, -extents.z, extents.z)};
}

inline glm::vec3
direction_in_cone(std::mt19937& rng, const glm::vec3& axis, float half_angle_rad)
{
    if (half_angle_rad <= 0.0f)
    {
        return glm::normalize(axis);
    }

    glm::vec3 n = glm::normalize(axis);
    glm::vec3 tangent = glm::abs(n.y) < 0.999f ? glm::normalize(glm::cross(n, glm::vec3{0, 1, 0}))
                                               : glm::normalize(glm::cross(n, glm::vec3{1, 0, 0}));
    glm::vec3 bitangent = glm::cross(n, tangent);

    float phi = uniform(rng, 0.0f, glm::two_pi<float>());
    float cos_theta_min = std::cos(half_angle_rad);
    float cos_theta = uniform(rng, cos_theta_min, 1.0f);
    float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));

    return glm::normalize(n * cos_theta + tangent * (std::cos(phi) * sin_theta) +
                          bitangent * (std::sin(phi) * sin_theta));
}

}  // namespace

emitter::emitter(emitter_params params)
    : m_params(std::move(params))
{
    m_particles.reserve(m_params.max_particles);
}

void
emitter::tick(float dt)
{
    if (!enabled)
    {
        return;
    }

    KRG_check(dt >= 0.0f, "negative dt");

    for (auto& p : m_particles)
    {
        p.age += dt;
        glm::vec3 a = m_params.acceleration - p.velocity * m_params.drag;
        p.velocity += a * dt;
        p.position += p.velocity * dt;

        float t = m_params.lifetime > 0.0f ? std::clamp(p.age / m_params.lifetime, 0.0f, 1.0f)
                                           : 1.0f;
        p.color = glm::mix(m_params.color_start, m_params.color_end, t);
        p.size = glm::mix(m_params.size_start, m_params.size_end, t);
    }

    std::erase_if(m_particles, [](const particle& p) { return p.age >= p.lifetime; });

    m_spawn_accumulator += m_params.spawn_rate * dt;
    while (m_spawn_accumulator >= 1.0f && m_particles.size() < m_params.max_particles)
    {
        spawn_one();
        m_spawn_accumulator -= 1.0f;
    }

    if (m_particles.size() >= m_params.max_particles)
    {
        m_spawn_accumulator = 0.0f;
    }
}

void
emitter::spawn_burst(std::uint32_t count)
{
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (m_particles.size() >= m_params.max_particles)
        {
            break;
        }
        spawn_one();
    }
}

void
emitter::clear()
{
    m_particles.clear();
    m_spawn_accumulator = 0.0f;
}

void
emitter::spawn_one()
{
    glm::vec3 spawn_pos = origin;
    switch (m_params.shape)
    {
    case spawn_shape::point:
        break;
    case spawn_shape::sphere:
        spawn_pos += random_point_in_sphere(m_rng, m_params.shape_extents.x);
        break;
    case spawn_shape::box:
        spawn_pos += random_point_in_box(m_rng, m_params.shape_extents);
        break;
    }

    glm::vec3 dir = direction_in_cone(
        m_rng, m_params.initial_velocity_dir, m_params.velocity_cone_angle_rad);
    float speed = m_params.velocity_speed +
                  uniform(m_rng, -m_params.velocity_speed_jitter, m_params.velocity_speed_jitter);

    float life = m_params.lifetime +
                 uniform(m_rng, -m_params.lifetime_jitter, m_params.lifetime_jitter);
    life = std::max(life, 0.01f);

    particle p;
    p.position = spawn_pos;
    p.velocity = dir * speed;
    p.color = m_params.color_start;
    p.size = m_params.size_start;
    p.age = 0.0f;
    p.lifetime = life;

    m_particles.push_back(p);
}

}  // namespace vfx
}  // namespace kryga

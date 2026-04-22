#pragma once

#include "vfx/particle.h"

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace kryga
{
namespace vfx
{

enum class spawn_shape : std::uint8_t
{
    point,
    sphere,
    box
};

struct emitter_params
{
    spawn_shape shape = spawn_shape::point;
    glm::vec3 shape_extents{0.0f};

    float spawn_rate = 20.0f;
    std::uint32_t max_particles = 1024;

    glm::vec3 initial_velocity_dir{0.0f, 1.0f, 0.0f};
    float velocity_cone_angle_rad = 0.0f;
    float velocity_speed = 1.0f;
    float velocity_speed_jitter = 0.0f;

    glm::vec3 acceleration{0.0f, -1.0f, 0.0f};
    float drag = 0.0f;

    float lifetime = 2.0f;
    float lifetime_jitter = 0.0f;

    glm::vec4 color_start{1.0f};
    glm::vec4 color_end{1.0f, 1.0f, 1.0f, 0.0f};
    float size_start = 0.1f;
    float size_end = 0.0f;
};

class emitter
{
public:
    explicit emitter(emitter_params params);

    void
    tick(float dt);

    void
    spawn_burst(std::uint32_t count);

    void
    clear();

    std::span<const particle>
    get_particles() const
    {
        return {m_particles.data(), m_particles.size()};
    }

    emitter_params&
    params()
    {
        return m_params;
    }

    const emitter_params&
    params() const
    {
        return m_params;
    }

    glm::vec3 origin{0.0f};
    bool enabled = true;

private:
    void
    spawn_one();

    emitter_params m_params;
    std::vector<particle> m_particles;
    float m_spawn_accumulator = 0.0f;
    std::mt19937 m_rng{0xA11CE};
};

}  // namespace vfx
}  // namespace kryga

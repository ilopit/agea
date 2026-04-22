#include "vfx/presets.h"

#include <utils/check.h>

#include <glm/gtc/constants.hpp>

namespace kryga
{
namespace vfx
{

emitter_params
make_preset(preset p)
{
    emitter_params r;
    switch (p)
    {
    case preset::particles:
        r.shape = spawn_shape::point;
        r.spawn_rate = 60.0f;
        r.max_particles = 512;
        r.initial_velocity_dir = {0.0f, 1.0f, 0.0f};
        r.velocity_cone_angle_rad = glm::pi<float>() * 0.25f;
        r.velocity_speed = 2.5f;
        r.velocity_speed_jitter = 0.5f;
        r.acceleration = {0.0f, -2.0f, 0.0f};
        r.drag = 0.2f;
        r.lifetime = 1.2f;
        r.lifetime_jitter = 0.2f;
        r.color_start = {1.0f, 0.9f, 0.4f, 1.0f};
        r.color_end = {1.0f, 0.3f, 0.0f, 0.0f};
        r.size_start = 0.12f;
        r.size_end = 0.02f;
        return r;

    case preset::smoke:
        r.shape = spawn_shape::sphere;
        r.shape_extents = {0.2f, 0.2f, 0.2f};
        r.spawn_rate = 25.0f;
        r.max_particles = 256;
        r.initial_velocity_dir = {0.0f, 1.0f, 0.0f};
        r.velocity_cone_angle_rad = glm::pi<float>() * 0.15f;
        r.velocity_speed = 0.8f;
        r.velocity_speed_jitter = 0.2f;
        r.acceleration = {0.0f, 0.3f, 0.0f};
        r.drag = 0.6f;
        r.lifetime = 3.5f;
        r.lifetime_jitter = 0.6f;
        r.color_start = {0.4f, 0.4f, 0.45f, 0.6f};
        r.color_end = {0.15f, 0.15f, 0.18f, 0.0f};
        r.size_start = 0.5f;
        r.size_end = 1.8f;
        return r;

    case preset::dust:
        r.shape = spawn_shape::box;
        r.shape_extents = {1.0f, 0.05f, 1.0f};
        r.spawn_rate = 40.0f;
        r.max_particles = 768;
        r.initial_velocity_dir = {0.0f, 1.0f, 0.0f};
        r.velocity_cone_angle_rad = glm::pi<float>() * 0.5f;
        r.velocity_speed = 0.15f;
        r.velocity_speed_jitter = 0.1f;
        r.acceleration = {0.0f, 0.05f, 0.0f};
        r.drag = 0.4f;
        r.lifetime = 5.0f;
        r.lifetime_jitter = 1.5f;
        r.color_start = {0.85f, 0.78f, 0.65f, 0.35f};
        r.color_end = {0.85f, 0.78f, 0.65f, 0.0f};
        r.size_start = 0.04f;
        r.size_end = 0.04f;
        return r;
    }

    KRG_never("unknown preset");
    return r;
}

const char*
preset_name(preset p)
{
    switch (p)
    {
    case preset::particles:
        return "particles";
    case preset::smoke:
        return "smoke";
    case preset::dust:
        return "dust";
    }
    return "unknown";
}

}  // namespace vfx
}  // namespace kryga

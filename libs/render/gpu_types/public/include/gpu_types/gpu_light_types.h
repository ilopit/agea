// GPU Light Types - Shared between C++ and GLSL
// Include gpu_types_macros.h before this file

#ifndef GPU_LIGHT_TYPES_H
#define GPU_LIGHT_TYPES_H

#include <gpu_types/gpu_port.h>

#define KGPU_light_type_spot 0
#define KGPU_light_type_point 1

GPU_BEGIN_NAMESPACE

// Directional light data (sun, moon, etc.)
struct directional_light_data
{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// Unified local light data (point + spot combined)
// type: 0 = point, 1 = spot

struct universal_light_data
{
    vec3 position;
    vec3 direction;  // unused for point lights
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    uint type;            // 0 = point, 1 = spot
    float cut_off;        // unused for point lights (set to -1)
    float outer_cut_off;  // unused for point lights
    float radius;

    uint slot;          // Index in the light buffer (for compute shader)
    uint shadow_index;  // Index into local_shadows[] or KGPU_SHADOW_INDEX_NONE
};

GPU_END_NAMESPACE

#ifdef __cplusplus
#include <cstddef>
static_assert(sizeof(kryga::gpu::universal_light_data) == 84,
              "universal_light_data size mismatch with GLSL scalar layout");
static_assert(offsetof(kryga::gpu::universal_light_data, position) == 0, "position offset");
static_assert(offsetof(kryga::gpu::universal_light_data, direction) == 12, "direction offset");
static_assert(offsetof(kryga::gpu::universal_light_data, ambient) == 24, "ambient offset");
static_assert(offsetof(kryga::gpu::universal_light_data, diffuse) == 36, "diffuse offset");
static_assert(offsetof(kryga::gpu::universal_light_data, specular) == 48, "specular offset");
static_assert(offsetof(kryga::gpu::universal_light_data, type) == 60, "type offset");
static_assert(offsetof(kryga::gpu::universal_light_data, cut_off) == 64, "cut_off offset");
static_assert(offsetof(kryga::gpu::universal_light_data, outer_cut_off) == 68,
              "outer_cut_off offset");
static_assert(offsetof(kryga::gpu::universal_light_data, radius) == 72, "radius offset");
static_assert(offsetof(kryga::gpu::universal_light_data, slot) == 76, "slot offset");
static_assert(offsetof(kryga::gpu::universal_light_data, shadow_index) == 80,
              "shadow_index offset");
#endif

#endif  // GPU_LIGHT_TYPES_H

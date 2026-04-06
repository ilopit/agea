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

#endif  // GPU_LIGHT_TYPES_H

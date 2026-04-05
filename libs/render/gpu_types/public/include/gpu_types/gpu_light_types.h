// GPU Light Types - Shared between C++ and GLSL
// Include gpu_types_macros.h before this file

#ifndef GPU_LIGHT_TYPES_H
#define GPU_LIGHT_TYPES_H

#include <gpu_types/gpu_port.h>

#define KGPU_light_type_spot 0
#define KGPU_light_type_point 1

GPU_BEGIN_NAMESPACE

// Directional light data (sun, moon, etc.)
std140_struct directional_light_data
{
    std140_vec3 direction;
    std140_vec3 ambient;
    std140_vec3 diffuse;
    std140_vec3 specular;
};

// Unified local light data (point + spot combined)
// type: 0 = point, 1 = spot

std140_struct universal_light_data
{
    std140_vec3 position;
    std140_vec3 direction;  // unused for point lights
    std140_vec3 ambient;
    std140_vec3 diffuse;
    std140_vec3 specular;

    std140_uint type;            // 0 = point, 1 = spot
    std140_float cut_off;        // unused for point lights (set to -1)
    std140_float outer_cut_off;  // unused for point lights
    std140_float radius;

    std140_uint slot;          // Index in the light buffer (for compute shader)
    std140_uint shadow_index;  // Index into local_shadows[] or KGPU_SHADOW_INDEX_NONE
};

GPU_END_NAMESPACE

#endif  // GPU_LIGHT_TYPES_H

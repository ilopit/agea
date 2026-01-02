// GPU Light Types - Shared between C++ and GLSL
// Include gpu_types_macros.h before this file

#ifndef GPU_LIGHT_TYPES_H
#define GPU_LIGHT_TYPES_H

#include <gpu_types/gpu_port.h>

#define KGPU_light_type_spot  0
#define KGPU_light_type_point 1

GPU_BEGIN_NAMESPACE

// Directional light data (sun, moon, etc.)
gpu_struct_std140 directional_light_data
{
    align_std140 vec3 direction;
    align_std140 vec3 ambient;
    align_std140 vec3 diffuse;
    align_std140 vec3 specular;
};

// Unified local light data (point + spot combined)
// type: 0 = point, 1 = spot

gpu_struct_std140 universal_light_data
{
    align_std140 vec3 position;
    align_std140 vec3 direction;  // unused for point lights
    align_std140 vec3 ambient;
    align_std140 vec3 diffuse;
    align_std140 vec3 specular;

    uint type;            // 0 = point, 1 = spot
    float cut_off;        // unused for point lights (set to -1)
    float outer_cut_off;  // unused for point lights
    float constant;
    float linear;
    float quadratic;
};

GPU_END_NAMESPACE

#endif  // GPU_LIGHT_TYPES_H

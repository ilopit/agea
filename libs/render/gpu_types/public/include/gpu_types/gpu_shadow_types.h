// GPU Shadow Types - Shared between C++ and GLSL

#ifndef GPU_SHADOW_TYPES_H
#define GPU_SHADOW_TYPES_H

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_generic_constants.h>

#define KGPU_CSM_CASCADE_COUNT 4
#define KGPU_CSM_CASCADE_COUNT_MIN 1
#define KGPU_CSM_CASCADE_COUNT_MAX KGPU_CSM_CASCADE_COUNT
#define KGPU_MAX_SHADOWED_LOCAL_LIGHTS 8
#define KGPU_SHADOW_MAP_SIZE 2048
#define KGPU_SHADOW_MAP_SIZE_MIN 256
#define KGPU_SHADOW_MAP_SIZE_MAX 8192
#define KGPU_SHADOW_INDEX_NONE 0xFFFFFFFFu

#define KGPU_SHADOW_BIAS_MIN 0.0f
#define KGPU_SHADOW_BIAS_MAX 0.1f
#define KGPU_SHADOW_NORMAL_BIAS_MIN 0.0f
#define KGPU_SHADOW_NORMAL_BIAS_MAX 0.5f
#define KGPU_SHADOW_DISTANCE_MIN 1.0f
#define KGPU_SHADOW_DISTANCE_MAX 5000.0f

GPU_BEGIN_NAMESPACE

struct shadow_cascade_data
{
    mat4 view_proj;
    float split_depth;
};

#define KGPU_PCF_3X3 0
#define KGPU_PCF_5X5 1
#define KGPU_PCF_7X7 2
#define KGPU_PCF_POISSON16 3
#define KGPU_PCF_POISSON32 4
#define KGPU_PCF_MIN KGPU_PCF_3X3
#define KGPU_PCF_MAX KGPU_PCF_POISSON32

struct directional_shadow_data
{
    shadow_cascade_data cascades[KGPU_CSM_CASCADE_COUNT];
    uvec4 shadow_map_indices;  // packed 4 cascade indices into uvec4
    uint cascade_count;
    float shadow_bias;
    float normal_bias;
    float texel_size;
    uint pcf_mode;
};

struct local_light_shadow_data
{
    mat4 view_proj;
    mat4 view_proj_back;
    uvec4 shadow_info;   // x=shadow_map_index, y=shadow_map_index_back, z=light_type,
                                // w=unused
    vec4 shadow_params;  // x=bias, y=normal_bias, z=texel_size, w=near_plane
    float far_plane;
};

struct shadow_config_data
{
    directional_shadow_data directional;
    local_light_shadow_data local_shadows[KGPU_MAX_SHADOWED_LOCAL_LIGHTS];
    uint shadowed_local_count;
};

GPU_END_NAMESPACE

#endif  // GPU_SHADOW_TYPES_H

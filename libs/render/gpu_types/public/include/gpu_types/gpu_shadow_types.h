// GPU Shadow Types - Shared between C++ and GLSL

#ifndef GPU_SHADOW_TYPES_H
#define GPU_SHADOW_TYPES_H

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_generic_constants.h>

#define KGPU_CSM_CASCADE_COUNT 4
#define KGPU_MAX_SHADOWED_LOCAL_LIGHTS 8
#define KGPU_SHADOW_MAP_SIZE 2048
#define KGPU_SHADOW_INDEX_NONE 0xFFFFFFFFu

GPU_BEGIN_NAMESPACE

gpu_struct_std140 shadow_cascade_data
{
    align_std140 mat4 view_proj;
    std140_float split_depth;
};

#define KGPU_PCF_3X3      0
#define KGPU_PCF_5X5      1
#define KGPU_PCF_7X7      2
#define KGPU_PCF_POISSON16 3
#define KGPU_PCF_POISSON32 4

gpu_struct_std140 directional_shadow_data
{
    shadow_cascade_data cascades[KGPU_CSM_CASCADE_COUNT];
    align_std140 uvec4 shadow_map_indices;   // packed 4 cascade indices into uvec4
    uint cascade_count;
    std140_float shadow_bias;
    std140_float normal_bias;
    std140_float texel_size;
    uint pcf_mode;
};

gpu_struct_std140 local_light_shadow_data
{
    align_std140 mat4 view_proj;
    align_std140 mat4 view_proj_back;
    align_std140 uvec4 shadow_info;     // x=shadow_map_index, y=shadow_map_index_back, z=light_type, w=unused
    align_std140 vec4 shadow_params;    // x=bias, y=normal_bias, z=texel_size, w=near_plane
    std140_float far_plane;
};

gpu_struct_std140 shadow_config_data
{
    directional_shadow_data directional;
    local_light_shadow_data local_shadows[KGPU_MAX_SHADOWED_LOCAL_LIGHTS];
    uint shadowed_local_count;
};

GPU_END_NAMESPACE

#endif  // GPU_SHADOW_TYPES_H

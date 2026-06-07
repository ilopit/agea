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
#define KGPU_SHADOW_MAP_SIZE_MIN 64
#define KGPU_SHADOW_MAP_SIZE_MAX 16384
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
    float texel_world_size;
    vec2 atlas_offset;
    vec2 atlas_scale;
};

#define KGPU_PCF_3X3 0
#define KGPU_PCF_5X5 1
#define KGPU_PCF_7X7 2
#define KGPU_PCF_POISSON16 3
#define KGPU_PCF_POISSON32 4
#define KGPU_PCF_POISSON64 5
// PCSS (Percentage-Closer Soft Shadows) — directional only. A fully separate
// code path with its own bias (pcss_* fields below); selecting it never affects
// the fixed-radius PCF modes above.
#define KGPU_PCF_PCSS 6
#define KGPU_PCF_MIN KGPU_PCF_3X3
#define KGPU_PCF_MAX KGPU_PCF_PCSS

#define KGPU_PCSS_LIGHT_SIZE_MIN 0.0f
#define KGPU_PCSS_LIGHT_SIZE_MAX 5.0f

struct directional_shadow_data
{
    shadow_cascade_data cascades[KGPU_CSM_CASCADE_COUNT];
    uint cascade_count;
    float shadow_bias;
    float normal_bias;
    float texel_size;
    uint pcf_mode;
    float pcf_world_radius;
    uint hardware_pcf;
    // xyz = directional light travel direction (world space). Used to apply the depth
    // bias as a world-space offset (shadow_bias is in METERS), so the bias is uniform
    // across cascades instead of scaling with each cascade's ortho depth range.
    vec4 light_dir;
    // --- PCSS-only state (used only when pcf_mode == KGPU_PCF_PCSS) ---
    // Kept fully separate from shadow_bias / normal_bias above so tuning the soft
    // path can never re-introduce acne in the fixed-radius PCF modes. Appended as
    // 4 floats after the vec4 (16-byte boundary): identical packing in C++ & std430.
    float pcss_light_size;    // penumbra scale, world units (light angular size)
    float pcss_bias;          // PCSS depth bias (meters), independent of shadow_bias
    float pcss_normal_bias;   // PCSS normal bias, independent of normal_bias
    float _pad_pcss;
};

struct local_light_shadow_data
{
    mat4 view_proj;
    mat4 view_proj_back;
    uvec4 shadow_info;   // x=unused, y=unused, z=light_type, w=unused
    vec4 shadow_params;  // x=bias, y=normal_bias, z=texel_size, w=near_plane
    float far_plane;
    float _pad0;
    vec2 atlas_offset_front;
    vec2 atlas_scale_front;
    vec2 atlas_offset_back;
    vec2 atlas_scale_back;
};

struct shadow_config_data
{
    directional_shadow_data directional;
    local_light_shadow_data local_shadows[KGPU_MAX_SHADOWED_LOCAL_LIGHTS];
    uint shadowed_local_count;
    uint atlas_bindless_index;
    uint hardware_pcf_local;
};

GPU_END_NAMESPACE

#endif  // GPU_SHADOW_TYPES_H

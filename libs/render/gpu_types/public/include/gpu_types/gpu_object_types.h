// GPU Object Types - Shared between C++ and GLSL

#ifndef GPU_OBJECT_TYPES_H
#define GPU_OBJECT_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// Object data for rendering - uses type-aware alignment macros
struct object_data
{
    mat4 model;
    mat4 normal;
    vec3 obj_pos;
    float bounding_radius;
    uint material_id;             // Index into material SSBO - enables per-object materials
    uint bone_offset;             // Offset into bone_matrices SSBO (0 = not skinned)
    uint bone_count;              // Number of bones (0 = not skinned)
    uint probe_index;             // Index into probe SSBO for indirect lighting (0xFFFFFFFF = none)
    vec2 lightmap_scale;          // Per-instance lightmap atlas scale
    vec2 lightmap_offset;         // Per-instance lightmap atlas offset
    uint lightmap_texture_index;  // Bindless texture index for lightmap (0xFFFFFFFF = none)
};

GPU_END_NAMESPACE

#endif  // GPU_OBJECT_TYPES_H

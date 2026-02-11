// GPU Object Types - Shared between C++ and GLSL

#ifndef GPU_OBJECT_TYPES_H
#define GPU_OBJECT_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// Object data for rendering - uses type-aware alignment macros
gpu_struct_std140 object_data
{
    std140_mat4  model;
    std140_mat4  normal;
    std140_vec3  obj_pos;
    std140_float bounding_radius;
    std140_uint  material_id;       // Index into material SSBO - enables per-object materials
    std140_uint  bone_offset;       // Offset into bone_matrices SSBO (0 = not skinned)
    std140_uint  bone_count;        // Number of bones (0 = not skinned)
};

GPU_END_NAMESPACE

#endif  // GPU_OBJECT_TYPES_H

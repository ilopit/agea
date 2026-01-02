// GPU Object Types - Shared between C++ and GLSL

#ifndef GPU_OBJECT_TYPES_H
#define GPU_OBJECT_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

gpu_struct_std140 object_data
{
    align_std140 mat4 model;
    align_std140 mat4 normal;
    align_std140 vec3 obj_pos;
};

GPU_END_NAMESPACE

#endif  // GPU_OBJECT_TYPES_H

// GPU Camera Types - Shared between C++ and GLSL
// Include gpu_types_macros.h before this file

#ifndef GPU_CAMERA_TYPES_H
#define GPU_CAMERA_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

gpu_struct_std140 camera_data
{
    align_std140 mat4 projection;
    align_std140 mat4 view;
    align_std140 vec3 position;
};

GPU_END_NAMESPACE

#endif  // GPU_CAMERA_TYPES_H

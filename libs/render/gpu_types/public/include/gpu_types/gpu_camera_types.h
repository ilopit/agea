// GPU Camera Types - Shared between C++ and GLSL
// Include gpu_types_macros.h before this file

#ifndef GPU_CAMERA_TYPES_H
#define GPU_CAMERA_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

std140_struct camera_data
{
    std140_mat4 projection;
    std140_mat4 inv_projection;
    std140_mat4 view;
    std140_vec3 position;
};

GPU_END_NAMESPACE

#endif  // GPU_CAMERA_TYPES_H

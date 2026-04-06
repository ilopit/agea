// GPU Camera Types - Shared between C++ and GLSL
// Include gpu_types_macros.h before this file

#ifndef GPU_CAMERA_TYPES_H
#define GPU_CAMERA_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

struct camera_data
{
    mat4 projection;
    mat4 inv_projection;
    mat4 view;
    vec3 position;
};

GPU_END_NAMESPACE

#endif  // GPU_CAMERA_TYPES_H

// GPU Vertex Types - Shared between C++ and GLSL

#ifndef GPU_VERTEX_TYPES_H
#define GPU_VERTEX_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

struct vertex_data
{
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
};

GPU_END_NAMESPACE

#endif  // GPU_VERTEX_TYPES_H

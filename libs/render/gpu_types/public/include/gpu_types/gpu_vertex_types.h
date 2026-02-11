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

struct skinned_vertex_data
{
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
    uvec4 bone_indices;
    vec4 bone_weights;
};

GPU_END_NAMESPACE

#endif  // GPU_VERTEX_TYPES_H

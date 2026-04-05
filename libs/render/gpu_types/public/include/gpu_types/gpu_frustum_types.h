// GPU Frustum Types - Shared between C++ and GLSL

#ifndef GPU_FRUSTUM_TYPES_H
#define GPU_FRUSTUM_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// Frustum represented as 6 planes (left, right, bottom, top, near, far)
// Each plane: vec4(normal.xyz, distance)
std140_struct frustum_data
{
    std140_vec4 planes[6];
};

// Indirect draw command (matches VkDrawIndexedIndirectCommand)
std140_struct draw_indexed_indirect_cmd
{
    std140_uint indexCount;
    std140_uint instanceCount;
    std140_uint firstIndex;
    std140_int vertexOffset;
    std140_uint firstInstance;
};

// Frustum cull output data
std140_struct cull_output_data
{
    std140_uint visible_count;
};

GPU_END_NAMESPACE

#endif  // GPU_FRUSTUM_TYPES_H

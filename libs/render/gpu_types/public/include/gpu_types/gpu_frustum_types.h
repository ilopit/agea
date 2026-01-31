// GPU Frustum Types - Shared between C++ and GLSL

#ifndef GPU_FRUSTUM_TYPES_H
#define GPU_FRUSTUM_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// Frustum represented as 6 planes (left, right, bottom, top, near, far)
// Each plane: vec4(normal.xyz, distance)
gpu_struct_std140 frustum_data
{
    align_std140 vec4 planes[6];
};

// Indirect draw command (matches VkDrawIndexedIndirectCommand)
gpu_struct_std140 draw_indexed_indirect_cmd
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

// Frustum cull output data
gpu_struct_std140 cull_output_data
{
    uint visible_count;
};

GPU_END_NAMESPACE

#endif  // GPU_FRUSTUM_TYPES_H

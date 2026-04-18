// Descriptor bindings for grid render pass shader — only camera UBO needed.

#extension GL_EXT_scalar_block_layout : require

#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_camera_types.h"

layout(set = KGPU_global_descriptor_sets, binding = 0, scalar) uniform CameraData
{
    camera_data obj;
} dyn_camera_data;

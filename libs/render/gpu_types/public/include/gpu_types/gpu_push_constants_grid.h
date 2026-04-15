// Push constants for grid pass
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_GRID_H
#define GPU_PUSH_CONSTANTS_GRID_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

struct push_constants_grid
{
    bda_addr bdag_camera;
};

GPU_END_NAMESPACE

#endif

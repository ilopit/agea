// Push constants for grid pass
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_GRID_H
#define GPU_PUSH_CONSTANTS_GRID_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

struct push_constants_grid
{
    bda_addr bdag_camera;
    // Bindless idx of main pass's depth attachment, or 0xFFFFFFFFu when grid
    // is drawn directly into the main pass (native depth test handles occlusion).
    // When valid, the grid fragment shader manually compares its computed depth
    // against the sampled scene depth and discards fragments behind geometry.
    uint scene_depth_idx;
    // Explicit padding — without it, glm::vec2 is 4-byte aligned in C++ while
    // std430 places vec2 at 8-byte boundaries in GLSL, giving the next field a
    // different offset on each side.
    uint _pad0;
    // 1.0/screen_width, 1.0/screen_height — used to derive the scene-depth UV
    // from gl_FragCoord inside the fragment shader.
    vec2 screen_size_inv;
};

GPU_END_NAMESPACE

#endif

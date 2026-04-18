// GPU BVH Types - Shared between C++ and GLSL
// Used by the lightmap baker compute shaders

#ifndef GPU_BVH_TYPES_H
#define GPU_BVH_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// BVH node for GPU traversal (flat array, 32 bytes per node)
// For internal nodes: left_or_tri_idx = left child index, right_or_count = right child index
// For leaf nodes: left_or_tri_idx = first triangle index, right_or_count = triangle count
struct bvh_node
{
    vec3 aabb_min;
    uint left_or_tri_idx;
    vec3 aabb_max;
    uint right_or_count;
};

// Triangle data for ray intersection in the baker (scalar layout, no padding needed)
struct bake_triangle
{
    vec3 v0;
    vec3 v1;
    vec3 v2;
    vec3 n0;
    vec3 n1;
    vec3 n2;
    vec2 lm_uv0;
    vec2 lm_uv1;
    vec2 lm_uv2;
};

// Baker configuration pushed to compute shaders
struct bake_config
{
    uint triangle_count;
    uint node_count;
    uint atlas_width;
    uint atlas_height;
    uint sample_count;
    uint bounce_count;
    float ao_radius;
    float ao_intensity;
    uint local_light_count;
    float shadow_bias;
    uint shadow_samples;
    float shadow_spread;
};

#define KGPU_BVH_LEAF_FLAG 0x80000000u
#define KGPU_BVH_IS_LEAF(node) (((node).right_or_count & KGPU_BVH_LEAF_FLAG) != 0u)
#define KGPU_BVH_TRI_COUNT(node) ((node).right_or_count & 0x7FFFFFFFu)

GPU_END_NAMESPACE

#endif  // GPU_BVH_TYPES_H

// GPU Cluster Types - Shared between C++ and GLSL
// Used for clustered lighting

#ifndef GPU_CLUSTER_TYPES_H
#define GPU_CLUSTER_TYPES_H

#include <gpu_types/gpu_port.h>

// Cluster grid constants
#define KGPU_cluster_tile_size 128
#define KGPU_cluster_depth_slices 12
#define KGPU_max_lights_per_cluster 32

// Descriptor set bindings for cluster data
#define KGPU_objects_cluster_light_counts_binding 3
#define KGPU_objects_cluster_light_indices_binding 4

GPU_BEGIN_NAMESPACE

// Cluster grid configuration - uploaded as uniform
gpu_struct_std140 cluster_grid_data
{
    uint tiles_x;
    uint tiles_y;
    uint depth_slices;
    uint tile_size;

    float near_plane;
    float far_plane;
    float log_depth_ratio;  // log(far/near) precomputed
    uint max_lights_per_cluster;

    uint screen_width;   // Actual framebuffer width (for correct NDC calculation)
    uint screen_height;  // Actual framebuffer height
};

GPU_END_NAMESPACE

#endif  // GPU_CLUSTER_TYPES_H

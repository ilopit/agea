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
std140_struct cluster_grid_data
{
    std140_uint tiles_x;
    std140_uint tiles_y;
    std140_uint depth_slices;
    std140_uint tile_size;

    std140_float near_plane;
    std140_float far_plane;
    std140_float log_depth_ratio;  // log(far/near) precomputed
    std140_uint max_lights_per_cluster;

    std140_uint screen_width;   // Actual framebuffer width (for correct NDC calculation)
    std140_uint screen_height;  // Actual framebuffer height
};

GPU_END_NAMESPACE

#endif  // GPU_CLUSTER_TYPES_H

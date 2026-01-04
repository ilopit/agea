#extension GL_GOOGLE_include_directive: enable
#include "gpu_types/gpu_light_types.h"
#include "gpu_types/gpu_camera_types.h"
#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_push_constants.h"
#include "gpu_types/gpu_cluster_types.h"

// Input
layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

// Output
layout (location = 0) out vec4 out_color;

// Constants
layout(push_constant) uniform Constants
{
    push_constants obj;
} constants;

// Bindings
layout (set = KGPU_global_descriptor_sets, binding = 0) uniform camera_vbo
{
   camera_data obj;
} dyn_camera_data;

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_directional_light_binding) readonly buffer DirLightBuffer{

    directional_light_data objects[];
} dyn_directional_lights_buffer;

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_universal_light_binding) readonly buffer LightDataBuffer{

    universal_light_data objects[];
} dyn_gpu_universal_light_data;

// Clustered lighting bindings
gpu_struct_std140 cluster_light_count_data {
    uint count;
};

gpu_struct_std140 cluster_light_index_data {
    uint index;
};

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_cluster_light_counts_binding) readonly buffer ClusterLightCountsBuffer{
    cluster_light_count_data objects[];
} dyn_cluster_light_counts;

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_cluster_light_indices_binding) readonly buffer ClusterLightIndicesBuffer{
    cluster_light_index_data objects[];
} dyn_cluster_light_indices;

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_cluster_config_binding) readonly buffer ClusterConfigBuffer{
    cluster_grid_data config;
} dyn_cluster_config;

// Cluster lighting helper functions
uint getDepthSlice(float viewDepth)
{
    if (viewDepth <= dyn_cluster_config.config.near_plane)
        return 0u;
    if (viewDepth >= dyn_cluster_config.config.far_plane)
        return dyn_cluster_config.config.depth_slices - 1u;

    float logDepth = log(viewDepth / dyn_cluster_config.config.near_plane);
    float t = logDepth / dyn_cluster_config.config.log_depth_ratio;
    return uint(t * float(dyn_cluster_config.config.depth_slices));
}

uint getClusterIndex(vec2 screenPos, float viewDepth)
{
    uint tileX = uint(screenPos.x) / dyn_cluster_config.config.tile_size;
    uint tileY = uint(screenPos.y) / dyn_cluster_config.config.tile_size;
    uint slice = getDepthSlice(viewDepth);

    // Clamp to valid range
    tileX = min(tileX, dyn_cluster_config.config.tiles_x - 1u);
    tileY = min(tileY, dyn_cluster_config.config.tiles_y - 1u);
    slice = min(slice, dyn_cluster_config.config.depth_slices - 1u);

    return slice * (dyn_cluster_config.config.tiles_x * dyn_cluster_config.config.tiles_y)
         + tileY * dyn_cluster_config.config.tiles_x
         + tileX;
}

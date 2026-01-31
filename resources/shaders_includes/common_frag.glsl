#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_nonuniform_qualifier : require
#include "gpu_types/gpu_light_types.h"
#include "gpu_types/gpu_camera_types.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_push_constants.h"
#include "gpu_types/gpu_cluster_types.h"

// Input
layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;
layout (location = 4) in flat uint in_object_idx;

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

// Object buffer - provides per-object data including material_id
layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_objects_binding) readonly buffer ObjectDataBuffer{
    object_data objects[];
} dyn_object_buffer;

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

// Static sampler array (set 2, binding 0) - immutable samplers for runtime selection
layout(set = KGPU_textures_descriptor_sets, binding = 0) uniform sampler static_samplers[KGPU_SAMPLER_COUNT];

// Bindless texture array (set 2, binding 1) - separate from samplers
// Note: Textures at binding 1 because variable descriptor count must be highest binding
layout(set = KGPU_textures_descriptor_sets, binding = 1) uniform texture2D bindless_textures[];

// Bindless texture sampling helper with runtime sampler selection
vec4 sample_bindless_texture(uint texture_idx, uint sampler_idx, vec2 uv)
{
    if (texture_idx == 0xFFFFFFFFu) // INVALID_BINDLESS_INDEX
        return vec4(1.0, 0.0, 1.0, 1.0); // Magenta for missing texture
    return texture(
        sampler2D(bindless_textures[nonuniformEXT(texture_idx)],
                  static_samplers[sampler_idx]),
        uv);
}

// Cluster lighting helper functions
uint getDepthSlice(float viewDepth)
{
    if (viewDepth <= dyn_cluster_config.config.near_plane)
        return 0u;

    float logDepth = log(viewDepth / dyn_cluster_config.config.near_plane);
    float t = logDepth / dyn_cluster_config.config.log_depth_ratio;

    // CRITICAL: enforce half-open range
    t = clamp(t, 0.0, 0.99999994);

    uint slice = uint(t * float(dyn_cluster_config.config.depth_slices));
    return min(slice, dyn_cluster_config.config.depth_slices - 1u);
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

// Helper to get material_id from current object (uses in_object_idx from vertex shader)
uint get_material_id()
{
    return dyn_object_buffer.objects[in_object_idx].material_id;
}

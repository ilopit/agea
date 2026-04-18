// Descriptor bindings for main render pass shaders.
// Provides dyn_X identifiers backed by descriptor sets (replacing BDA pointers).
//
// Layout:
//   set 0, binding 0: camera UBO (vertex + fragment)
//   set 1, bindings 0..10: per-frame SSBOs / UBOs (objects, lights, clusters, instances,
//                          bones, shadow, probes)
//   set 2: bindless textures + static samplers (declared by common_frag.glsl)
//   set 3, binding 0: per-material SSBO (declared by each fragment shader with its
//                     material struct type)

#extension GL_EXT_scalar_block_layout : require

#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_camera_types.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_light_types.h"
#include "gpu_types/gpu_cluster_types.h"
#include "gpu_types/gpu_shadow_types.h"
#include "gpu_types/gpu_probe_types.h"

layout(set = KGPU_global_descriptor_sets, binding = 0, scalar) uniform CameraData
{
    camera_data obj;
} dyn_camera_data;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_objects_binding,
       scalar) readonly buffer ObjectBuffer
{
    object_data objects[];
} dyn_object_buffer;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_directional_light_binding,
       scalar) readonly buffer DirectionalLightBuffer
{
    directional_light_data objects[];
} dyn_directional_lights_buffer;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_universal_light_binding,
       scalar) readonly buffer UniversalLightBuffer
{
    universal_light_data objects[];
} dyn_gpu_universal_light_data;

struct cluster_light_count_data { uint count; };
struct cluster_light_index_data { uint index; };

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_cluster_light_counts_binding,
       scalar) readonly buffer ClusterLightCounts
{
    cluster_light_count_data objects[];
} dyn_cluster_light_counts;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_cluster_light_indices_binding,
       scalar) readonly buffer ClusterLightIndices
{
    cluster_light_index_data objects[];
} dyn_cluster_light_indices;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_cluster_config_binding,
       scalar) uniform ClusterConfig
{
    cluster_grid_data config;
} dyn_cluster_config;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_instance_slots_binding,
       scalar) readonly buffer InstanceSlots
{
    uint slots[];
} dyn_instance_slots;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_bone_matrices_binding,
       scalar) readonly buffer BoneMatrices
{
    mat4 matrices[];
} dyn_bone_matrices;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_shadow_data_binding,
       scalar) readonly buffer ShadowDataBuffer
{
    shadow_config_data shadow;
} dyn_shadow_data;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_probe_data_binding,
       scalar) readonly buffer ProbeDataBuffer
{
    sh_probe probes[];
} dyn_probe_data;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_probe_grid_binding,
       scalar) readonly buffer ProbeGridBuffer
{
    probe_grid_config grid;
} dyn_probe_grid;

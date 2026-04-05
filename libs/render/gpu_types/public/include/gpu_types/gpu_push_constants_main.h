// Push constants for main render pass (lit, unlit, outline, debug_wire)
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_MAIN_H
#define GPU_PUSH_CONSTANTS_MAIN_H

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_generic_constants.h>

#ifndef __cplusplus
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#endif

GPU_BEGIN_NAMESPACE

push_struct push_constants_main
{
    push_uint material_id;
    push_uint directional_light_id;
    push_uint use_clustered_lighting;
    push_uint instance_base;
    push_uint local_lights_size;
    push_uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    push_uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    push_uint local_light_ids[KGPU_max_lights_per_object];
    push_uint64 bdag_camera;
    push_uint64 bdag_objects;
    push_uint64 bdag_directional_lights;
    push_uint64 bdag_universal_lights;
    push_uint64 bdag_cluster_counts;
    push_uint64 bdag_cluster_indices;
    push_uint64 bdag_cluster_config;
    push_uint64 bdag_instance_slots;
    push_uint64 bdag_bone_matrices;
    push_uint64 bdag_shadow_data;
    push_uint64 bdaf_material;
};

GPU_END_NAMESPACE

#endif

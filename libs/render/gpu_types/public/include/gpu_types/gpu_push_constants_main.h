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

gpu_struct_pc push_constants_main
{
    uint material_id;
    uint directional_light_id;
    uint use_clustered_lighting;
    uint instance_base;
    uint local_lights_size;
    uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint local_light_ids[KGPU_max_lights_per_object];
    pc_uint64 bdag_camera;
    pc_uint64 bdag_objects;
    pc_uint64 bdag_directional_lights;
    pc_uint64 bdag_universal_lights;
    pc_uint64 bdag_cluster_counts;
    pc_uint64 bdag_cluster_indices;
    pc_uint64 bdag_cluster_config;
    pc_uint64 bdag_instance_slots;
    pc_uint64 bdag_bone_matrices;
    pc_uint64 bdag_shadow_data;
    pc_uint64 bdaf_material;
};

GPU_END_NAMESPACE

#endif

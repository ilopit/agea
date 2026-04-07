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

struct push_constants_main
{
    uint material_id;
    uint directional_light_id;
    uint use_clustered_lighting;
    uint instance_base;
    uint local_lights_size;
    uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint local_light_ids[KGPU_max_lights_per_object];
    uint64_t bdag_camera;
    uint64_t bdag_objects;
    uint64_t bdag_directional_lights;
    uint64_t bdag_universal_lights;
    uint64_t bdag_cluster_counts;
    uint64_t bdag_cluster_indices;
    uint64_t bdag_cluster_config;
    uint64_t bdag_instance_slots;
    uint64_t bdag_bone_matrices;
    uint64_t bdag_shadow_data;
    uint64_t bdag_probe_data;
    uint64_t bdag_probe_grid;
    uint64_t bdaf_material;
};

GPU_END_NAMESPACE

#endif

// Push constants for main render pass (lit, unlit, outline, debug_wire)
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_MAIN_H
#define GPU_PUSH_CONSTANTS_MAIN_H

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_generic_constants.h>

GPU_BEGIN_NAMESPACE

struct push_constants_main
{
    uint material_id;
    uint directional_light_id;
    uint use_clustered_lighting;
    uint instance_base;
    uint enable_directional_light;
    uint enable_local_lights;
    uint enable_baked_light;
    uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    bda_addr bdag_camera;
    bda_addr bdag_objects;
    bda_addr bdag_directional_lights;
    bda_addr bdag_universal_lights;
    bda_addr bdag_cluster_counts;
    bda_addr bdag_cluster_indices;
    bda_addr bdag_cluster_config;
    bda_addr bdag_instance_slots;
    bda_addr bdag_bone_matrices;
    bda_addr bdag_shadow_data;
    bda_addr bdag_probe_data;
    bda_addr bdag_probe_grid;
    bda_addr bdaf_material;
};

GPU_END_NAMESPACE

#endif

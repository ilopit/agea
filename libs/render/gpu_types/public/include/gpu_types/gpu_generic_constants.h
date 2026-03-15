#ifndef GPU_GENERIC_CONSTANTS_H
#define GPU_GENERIC_CONSTANTS_H

#include <gpu_types/gpu_port.h>

#define KGPU_global_descriptor_sets 0
#define KGPU_objects_descriptor_sets 1
#define KGPU_objects_objects_binding 0
#define KGPU_objects_directional_light_binding 1
#define KGPU_objects_universal_light_binding 2
#define KGPU_objects_cluster_light_counts_binding 3
#define KGPU_objects_cluster_light_indices_binding 4
#define KGPU_objects_cluster_config_binding 5
#define KGPU_objects_instance_slots_binding 6
#define KGPU_objects_bone_matrices_binding 7
#define KGPU_objects_shadow_data_binding 8
#define KGPU_objects_max_binding KGPU_objects_shadow_data_binding
#define KGPU_textures_descriptor_sets 2
#define KGPU_materials_descriptor_sets 3

#define KGPU_max_lights_per_object 32

#define KGPU_initial_instance_slots_size 8192

#define KGPU_znear 0.1
#define KGPU_zfar 2000

// Bindless textures
#define KGPU_max_bindless_textures 4096

// Static sampler types for runtime sampler selection
#define KGPU_SAMPLER_LINEAR_REPEAT       0  // Default for most textures
#define KGPU_SAMPLER_LINEAR_CLAMP        1  // Skyboxes, LUTs
#define KGPU_SAMPLER_LINEAR_MIRROR       2  // Seamless tiling
#define KGPU_SAMPLER_NEAREST_REPEAT      3  // Pixel art, data textures
#define KGPU_SAMPLER_NEAREST_CLAMP       4  // UI, fonts
#define KGPU_SAMPLER_LINEAR_CLAMP_BORDER 5  // Shadow maps
#define KGPU_SAMPLER_ANISO_REPEAT        6  // High-quality surfaces
#define KGPU_SAMPLER_COUNT               7

#endif

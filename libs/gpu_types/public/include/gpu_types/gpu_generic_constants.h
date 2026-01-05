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
#define KGPU_objects_max_binding KGPU_objects_cluster_config_binding
#define KGPU_textures_descriptor_sets 2
#define KGPU_materials_descriptor_sets 3

#define KGPU_max_lights_per_object 32

#endif

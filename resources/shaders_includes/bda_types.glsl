// BDA buffer reference TYPE declarations
// Include AFTER extensions are enabled. Does NOT define dyn_X macros —
// those are defined per-pass in bda_macros_*.glsl files.

#include "gpu_types/gpu_camera_types.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_light_types.h"
#include "gpu_types/gpu_cluster_types.h"
#include "gpu_types/gpu_shadow_types.h"

layout(buffer_reference, std140) readonly buffer BdaCameraRef {
    camera_data obj;
};

layout(buffer_reference, std140) readonly buffer BdaObjectRef {
    object_data objects[];
};

layout(buffer_reference, std140) readonly buffer BdaDirLightRef {
    directional_light_data objects[];
};

layout(buffer_reference, std140) readonly buffer BdaUniversalLightRef {
    universal_light_data objects[];
};

struct cluster_light_count_data { uint count; };
struct cluster_light_index_data { uint index; };

layout(buffer_reference, std140) readonly buffer BdaClusterCountsRef {
    cluster_light_count_data objects[];
};

layout(buffer_reference, std140) readonly buffer BdaClusterIndicesRef {
    cluster_light_index_data objects[];
};

layout(buffer_reference, std140) readonly buffer BdaClusterConfigRef {
    cluster_grid_data config;
};

layout(buffer_reference, std430) readonly buffer BdaInstanceSlotsRef {
    uint slots[];
};

layout(buffer_reference, std430) readonly buffer BdaBoneMatricesRef {
    mat4 matrices[];
};

layout(buffer_reference, std140) readonly buffer BdaShadowDataRef {
    shadow_config_data shadow;
};

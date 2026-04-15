// BDA buffer reference TYPE declarations
// Include AFTER extensions are enabled. Does NOT define dyn_X macros —
// those are defined per-pass in bda_macros_*.glsl files.

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include "gpu_types/gpu_camera_types.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_light_types.h"
#include "gpu_types/gpu_cluster_types.h"
#include "gpu_types/gpu_shadow_types.h"
#include "gpu_types/gpu_probe_types.h"

layout(buffer_reference, scalar) readonly buffer BdaCameraRef {
    camera_data obj;
};

layout(buffer_reference, scalar) readonly buffer BdaObjectRef {
    object_data objects[];
};

layout(buffer_reference, scalar) readonly buffer BdaDirLightRef {
    directional_light_data objects[];
};

layout(buffer_reference, scalar) readonly buffer BdaUniversalLightRef {
    universal_light_data objects[];
};

struct cluster_light_count_data { uint count; };
struct cluster_light_index_data { uint index; };

layout(buffer_reference, scalar) readonly buffer BdaClusterCountsRef {
    cluster_light_count_data objects[];
};

layout(buffer_reference, scalar) readonly buffer BdaClusterIndicesRef {
    cluster_light_index_data objects[];
};

layout(buffer_reference, scalar) readonly buffer BdaClusterConfigRef {
    cluster_grid_data config;
};

layout(buffer_reference, scalar) readonly buffer BdaInstanceSlotsRef {
    uint slots[];
};

layout(buffer_reference, scalar) readonly buffer BdaBoneMatricesRef {
    mat4 matrices[];
};

layout(buffer_reference, scalar) readonly buffer BdaShadowDataRef {
    shadow_config_data shadow;
};

layout(buffer_reference, scalar) readonly buffer BdaProbeDataRef {
    sh_probe probes[];
};

layout(buffer_reference, scalar) readonly buffer BdaProbeGridRef {
    probe_grid_config grid;
};

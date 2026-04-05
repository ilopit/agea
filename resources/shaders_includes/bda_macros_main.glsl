// BDA macros for main pass — requires push_constants_main declared as constants.obj
#include "bda_types.glsl"

#define dyn_camera_data                BdaCameraRef(constants.obj.bdag_camera)
#define dyn_object_buffer              BdaObjectRef(constants.obj.bdag_objects)
#define dyn_directional_lights_buffer  BdaDirLightRef(constants.obj.bdag_directional_lights)
#define dyn_gpu_universal_light_data   BdaUniversalLightRef(constants.obj.bdag_universal_lights)
#define dyn_cluster_light_counts       BdaClusterCountsRef(constants.obj.bdag_cluster_counts)
#define dyn_cluster_light_indices      BdaClusterIndicesRef(constants.obj.bdag_cluster_indices)
#define dyn_cluster_config             BdaClusterConfigRef(constants.obj.bdag_cluster_config)
#define dyn_instance_slots             BdaInstanceSlotsRef(constants.obj.bdag_instance_slots)
#define dyn_bone_matrices              BdaBoneMatricesRef(constants.obj.bdag_bone_matrices)
#define dyn_shadow_data                BdaShadowDataRef(constants.obj.bdag_shadow_data)

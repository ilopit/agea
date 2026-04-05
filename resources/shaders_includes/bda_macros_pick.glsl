// BDA macros for pick pass — requires push_constants_pick declared as constants.obj
#include "bda_types.glsl"

#define dyn_camera_data      BdaCameraRef(constants.obj.bdag_camera)
#define dyn_object_buffer    BdaObjectRef(constants.obj.bdag_objects)
#define dyn_instance_slots   BdaInstanceSlotsRef(constants.obj.bdag_instance_slots)
#define dyn_bone_matrices    BdaBoneMatricesRef(constants.obj.bdag_bone_matrices)

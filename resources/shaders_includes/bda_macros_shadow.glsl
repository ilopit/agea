// BDA macros for shadow pass — requires push_constants_shadow declared as constants.obj
#include "bda_types.glsl"

#define dyn_object_buffer    BdaObjectRef(constants.obj.bdag_objects)
#define dyn_instance_slots   BdaInstanceSlotsRef(constants.obj.bdag_instance_slots)
#define dyn_shadow_data      BdaShadowDataRef(constants.obj.bdag_shadow_data)

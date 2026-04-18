// Descriptor bindings for shadow render pass shaders.
// Subset of descriptor_bindings_common.glsl — shadow shaders only need objects,
// instance_slots, and shadow_data.

#extension GL_EXT_scalar_block_layout : require

#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_shadow_types.h"

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_objects_binding,
       scalar) readonly buffer ObjectBuffer
{
    object_data objects[];
} dyn_object_buffer;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_instance_slots_binding,
       scalar) readonly buffer InstanceSlots
{
    uint slots[];
} dyn_instance_slots;

layout(set = KGPU_objects_descriptor_sets,
       binding = KGPU_objects_shadow_data_binding,
       scalar) readonly buffer ShadowDataBuffer
{
    shadow_config_data shadow;
} dyn_shadow_data;

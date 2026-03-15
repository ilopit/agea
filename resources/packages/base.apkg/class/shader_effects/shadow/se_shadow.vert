#version 450
#extension GL_GOOGLE_include_directive : require
#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_shadow_types.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

#include "gpu_types/gpu_camera_types.h"

// Camera UBO (set 0 for layout compatibility - not used but binding must match)
layout (set = KGPU_global_descriptor_sets, binding = 0) uniform camera_vbo {
    camera_data obj;
} dyn_camera_data;

// Object buffer
layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_objects_binding) readonly buffer object_data_buffer {
    object_data objects[];
} dyn_object_buffer;

// Instance slots buffer
layout(std430, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_instance_slots_binding)
readonly buffer InstanceSlotsBuffer {
    uint slots[];
} dyn_instance_slots;

// Bone matrices SSBO (for set 1 layout compatibility)
layout(std430, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_bone_matrices_binding)
readonly buffer BoneMatricesBuffer {
    mat4 matrices[];
} dyn_bone_matrices;

// Shadow data SSBO
layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_shadow_data_binding) readonly buffer ShadowDataBuffer {
    shadow_config_data shadow;
} dyn_shadow_data;

#include "gpu_types/gpu_push_constants.h"

layout(push_constant) uniform Constants {
    push_constants obj;
} constants;

uint get_object_index() {
    return dyn_instance_slots.slots[constants.obj.instance_base + gl_InstanceIndex];
}

void main()
{
    uint obj_idx = get_object_index();
    mat4 model = dyn_object_buffer.objects[obj_idx].model;

    // directional_light_id encodes the shadow index (cascade or local light)
    // use_clustered_lighting encodes the mode: 0 = CSM cascade, 1 = local light
    uint shadow_idx = constants.obj.directional_light_id;
    mat4 light_vp;

    if (constants.obj.use_clustered_lighting == 0u)
        light_vp = dyn_shadow_data.shadow.directional.cascades[shadow_idx].view_proj;
    else
        light_vp = dyn_shadow_data.shadow.local_shadows[shadow_idx].view_proj;

    gl_Position = light_vp * model * vec4(in_position, 1.0);
}

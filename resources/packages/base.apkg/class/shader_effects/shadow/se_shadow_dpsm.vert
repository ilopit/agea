#version 450
#extension GL_GOOGLE_include_directive : require
#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_shadow_types.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out float out_depth;

#include "gpu_types/gpu_camera_types.h"

// Camera UBO (set 0 for layout compatibility)
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

    // Use directional_light_id field to encode shadow index
    uint shadow_idx = constants.obj.directional_light_id;

    // Light view matrix (no projection - paraboloid done here)
    mat4 lightView = dyn_shadow_data.shadow.local_shadows[shadow_idx].view_proj;
    float nearPlane = dyn_shadow_data.shadow.local_shadows[shadow_idx].shadow_params.w;
    float farPlane = dyn_shadow_data.shadow.local_shadows[shadow_idx].far_plane;

    // Transform to light space
    vec4 worldPos = model * vec4(in_position, 1.0);
    vec3 L = (lightView * worldPos).xyz;

    // use_clustered_lighting field reused as hemisphere selector (0=front, 1=back)
    bool backHemisphere = (constants.obj.use_clustered_lighting != 0u);
    if (backHemisphere)
        L.z = -L.z;

    // Normalize direction
    float dist = length(L);
    vec3 Ln = L / max(dist, 0.0001);

    // Dual-paraboloid projection
    // xy = Ln.xy / (Ln.z + 1.0)
    // z = linear depth
    float denom = Ln.z + 1.0;

    // Clip vertices behind paraboloid
    if (denom < 0.001)
    {
        gl_Position = vec4(0.0, 0.0, -1.0, 1.0);  // clip
        out_depth = 0.0;
        return;
    }

    vec2 clipXY = Ln.xy / denom;
    float depth = (dist - nearPlane) / (farPlane - nearPlane);
    depth = clamp(depth, 0.0, 1.0);

    gl_Position = vec4(clipXY, depth, 1.0);
    out_depth = depth;
}

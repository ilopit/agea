#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "gpu_types/gpu_push_constants_shadow.h"
layout(push_constant) uniform Constants { push_constants_shadow obj; } constants;
#include "bda_macros_shadow.glsl"

#include "gpu_types/gpu_generic_constants.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;
layout (location = 4) in vec2 in_lightmap_uv;

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

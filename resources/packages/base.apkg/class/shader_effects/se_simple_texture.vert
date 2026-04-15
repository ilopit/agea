#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_vert.glsl"

void main()
{
    uint obj_idx = get_object_index(constants.obj.instance_base);
    mat4 modelMatrix   = dyn_object_buffer.objects[obj_idx].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[obj_idx].normal;

    mat4 modelView = dyn_camera_data.obj.view * modelMatrix;

    out_object_idx = obj_idx;
    out_color    = in_color;
    out_tex_coord = in_tex_coord;
    out_normal   = mat3(normalMatrix) * in_normal;
    out_world_pos  = vec3(modelMatrix * vec4(in_position, 1));
    out_lightmap_uv = vec2(0);

    gl_Position =  dyn_camera_data.obj.projection * modelView * vec4(in_position, 1.0);
}

#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_vert.glsl"

void main()
{
    uint obj_idx = get_object_index(constants.obj.instance_base);
    mat4 modelMatrix   = dyn_object_buffer.objects[obj_idx].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[obj_idx].normal;

    // Scale by distance to camera, clamped to prevent billboards
    // from becoming too large when far away or too small when close
    vec3 world_pos = vec3(modelMatrix[3]);
    vec3 cam_pos = dyn_camera_data.obj.position;
    float dist = length(world_pos - cam_pos);
    float fixed_scale = clamp(dist * 0.05, 0.3, 1.5);

    mat4 modelView = dyn_camera_data.obj.view * modelMatrix;

    // Strip rotation — billboard faces camera
    modelView[0][0] = fixed_scale;
    modelView[0][1] = 0.0;
    modelView[0][2] = 0.0;

    modelView[1][0] = 0.0;
    modelView[1][1] = fixed_scale;
    modelView[1][2] = 0.0;

    modelView[2][0] = 0.0;
    modelView[2][1] = 0.0;
    modelView[2][2] = fixed_scale;

    out_object_idx = obj_idx;
    out_color    = in_color;
    out_tex_coord = in_tex_coord;
    out_normal   = mat3(normalMatrix) * in_normal;
    out_world_pos  = vec3(modelMatrix * vec4(in_position, 1));
    out_lightmap_uv = vec2(0);

    gl_Position = dyn_camera_data.obj.projection * modelView * vec4(in_position, 1.0);
}

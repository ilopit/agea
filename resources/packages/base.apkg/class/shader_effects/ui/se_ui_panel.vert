#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_vert.glsl"

// UI panel vertex shader.
//
// Treats object_data.model as a direct NDC transform — camera view/projection
// are intentionally NOT applied. plane_mesh is a (-1,-1)..(1,1) quad in x/y,
// so the final NDC position is modelMatrix applied to the mesh vertex.
//
// gl_Position.z is forced to a near-front depth so panels render on top of
// 3D content regardless of where the model matrix places the quad in z.
void main()
{
    uint obj_idx = get_object_index(constants.obj.instance_base);
    mat4 modelMatrix = dyn_object_buffer.objects[obj_idx].model;

    out_object_idx = obj_idx;
    out_color = in_color;
    out_tex_coord = in_tex_coord;
    out_normal = vec3(0.0, 0.0, 1.0);
    out_world_pos = vec3(0.0);
    out_lightmap_uv = vec2(0.0);

    vec4 ndc = modelMatrix * vec4(in_position, 1.0);
    gl_Position = vec4(ndc.xy, 0.01, 1.0);
}

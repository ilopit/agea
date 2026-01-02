#version 450
#include "common_vert.glsl"

void main() 
{
    mat4 modelMatrix   = dyn_object_buffer.objects[gl_InstanceIndex].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[gl_InstanceIndex].normal;

    mat4 modelView = dyn_camera_data.obj.view * modelMatrix;

    out_color      = in_color;
    out_tex_coord  = in_tex_coord;
    out_normal     = mat3(normalMatrix) * in_normal;
    out_world_pos  = vec3(modelMatrix * vec4(in_position, 1));

    gl_Position =  dyn_camera_data.obj.projection * modelView * vec4(in_position, 1.0);
}

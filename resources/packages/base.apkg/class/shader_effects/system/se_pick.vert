#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_vert.glsl"


out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
    mat4 modelMatrix   = dyn_object_buffer.objects[gl_InstanceIndex].model;
    mat4 modelView = dyn_camera_data.view * modelMatrix;

    out_color.b  = float( (gl_InstanceIndex >> 16) & 0xFF) / 255.0;
    out_color.g  = float( (gl_InstanceIndex >> 8) & 0xFF) / 255.0;
    out_color.r  = float( (gl_InstanceIndex) & 0xFF) / 255.0;


    gl_Position =  dyn_camera_data.projection * modelView * vec4(in_position, 1.0);
}
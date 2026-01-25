#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_vert.glsl"


out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
    uint obj_idx = get_object_index();
    mat4 modelMatrix   = dyn_object_buffer.objects[obj_idx].model;
    mat4 modelView = dyn_camera_data.obj.view * modelMatrix;

    // Encode actual object slot (not gl_InstanceIndex) for picking
    out_color.b  = float( (obj_idx >> 16) & 0xFFu) / 255.0;
    out_color.g  = float( (obj_idx >> 8) & 0xFFu) / 255.0;
    out_color.r  = float( (obj_idx) & 0xFFu) / 255.0;

    gl_Position =  dyn_camera_data.obj.projection * modelView * vec4(in_position, 1.0);
}
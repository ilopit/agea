#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_vert.glsl"

void main()
{
    uint obj_idx = get_object_index();
    mat4 modelMatrix   = dyn_object_buffer.objects[obj_idx].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[obj_idx].normal;

    mat4 modelView = dyn_camera_data.obj.view * modelMatrix;

    // First colunm.
    modelView[0][0] = 1.0;
    modelView[0][1] = 0.0;
    modelView[0][2] = 0.0;

    //if (spherical == 1)
    {
        // Second colunm.
        modelView[1][0] = 0.0;
        modelView[1][1] = 1.0;
        modelView[1][2] = 0.0;
    }

    // Thrid colunm.
    modelView[2][0] = 0.0;
    modelView[2][1] = 0.0;
    modelView[2][2] = 1.0;

    out_object_idx = obj_idx;
    out_color    = in_color;
    out_tex_coord = in_tex_coord;
    out_normal   = mat3(normalMatrix) * in_normal;
    out_world_pos  = vec3(modelMatrix * vec4(in_position, 1));

    gl_Position = dyn_camera_data.obj.projection * modelView * vec4(in_position, 1.0);
}

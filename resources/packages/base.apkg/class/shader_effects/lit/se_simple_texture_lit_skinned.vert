#version 450
#include "common_vert_skinned.glsl"

void main()
{
    uint obj_idx = get_object_index();
    mat4 modelMatrix   = dyn_object_buffer.objects[obj_idx].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[obj_idx].normal;

    // Apply skeletal skinning
    mat4 skinMatrix = get_skin_matrix(obj_idx);
    vec4 skinnedPos = skinMatrix * vec4(in_position, 1.0);
    vec3 skinnedNormal = mat3(skinMatrix) * in_normal;

    mat4 modelView = dyn_camera_data.obj.view * modelMatrix;

    out_object_idx = obj_idx;
    out_color      = in_color;
    out_tex_coord  = in_tex_coord;
    out_normal     = mat3(normalMatrix) * skinnedNormal;
    out_world_pos  = vec3(modelMatrix * skinnedPos);

    gl_Position = dyn_camera_data.obj.projection * modelView * skinnedPos;
}

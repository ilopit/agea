#version 450

#include "common_vert.glsl"

void main() 
{
    mat4 modelMatrix   = dyn_object_buffer.objects[gl_InstanceIndex].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[gl_InstanceIndex].normal;

    mat4 modelView = dyn_camera_data.view * modelMatrix;

    outColor    = vColor;
    outTexCoord = vTexCoord;
    outNormal   = mat3(normalMatrix) * vNormal;
    outWorldPos  = vec3(modelMatrix * vec4(vPosition, 1));

    gl_Position =  dyn_camera_data.projection * modelView * vec4(vPosition, 1.0);
}
#version 450
#include "common_vert.glsl"

void main() 
{
    mat4 modelMatrix   = dyn_object_buffer.objects[gl_InstanceIndex].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[gl_InstanceIndex].normal;

    mat4 modelView = dyn_camera_data.view * modelMatrix;

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
    
    outColor    = vColor;
    outTexCoord = vTexCoord;
    outNormal   = mat3(normalMatrix) * vNormal;
    outWorldPos  = vec3(modelMatrix * vec4(vPosition, 1));

    gl_Position = dyn_camera_data.projection * modelView * vec4(vPosition, 1.0);
}

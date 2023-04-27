#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec2 outTexCoord;

layout (set = 0, binding = 0) uniform CameraBuffer 
{
    mat4 projection;
    mat4 view;
    vec3 camPos;
} dyn_camera_data;

struct ObjectData{
    mat4 model;
    mat4 normal;
    vec3 objPos;
}; 

//all object matrices
layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{   

    ObjectData objects[];
} dyn_object_buffer;

void main() 
{
    mat4 modelMatrix   = dyn_object_buffer.objects[gl_InstanceIndex].model;
    mat4 normalMatrix  = dyn_object_buffer.objects[gl_InstanceIndex].normal;

    mat4 modelView = dyn_camera_data.view * modelMatrix;
    
    // First colunm.
    modelView[0][0] = 1.0; 
    modelView[0][1] = 0.0; 
    modelView[0][2] = 0.0; 

    if (spherical == 1)
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

    gl_Position =  dyn_camera_data.projection * modelView * vec4(vPosition, 1.0);
}

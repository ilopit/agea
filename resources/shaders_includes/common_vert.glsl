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

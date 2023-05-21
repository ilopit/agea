// Input
layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inTexCoord;

// Output
layout (location = 0) out vec4 outColor;

// Bindings
layout (set = 0, binding = 0) uniform CameraData 
{
    mat4 projection;
    mat4 view;
    vec3 camPos;
} dyn_camera_data;

layout(set = 0, binding = 1) uniform SceneData{
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} dyn_scene_data;

// Constants
layout(push_constant) uniform Constants
{
    uint material_id;
} constants;
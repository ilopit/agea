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

// Constants
layout(push_constant) uniform Constants
{
    uint material_id;
    uint directional_light_id;

    uint point_lights_size;
    uint point_light_ids[10];

    uint spot_lights_size;
    uint spot_light_ids[10];
} constants;

struct DirLight {
    vec3 direction;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;
    vec3 direction;  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular; 

    float cutOff;
    float outerCutOff;
  
    float constant;
    float linear;
    float quadratic;      
};

layout(set = 0, binding = 1) uniform SceneData{
    DirLight directional;
    PointLight points[10];
    SpotLight spots[10];
} dyn_scene_data;

layout(std140, set = 3, binding = 0) readonly buffer DirLightBuffer{   

    DirLight objects[];
} dyn_directional_lights_buffer;

layout(std140, set = 3, binding = 1) readonly buffer PointLightBuffer{   

    PointLight objects[];
} dyn_point_lights_buffer;

layout(std140, set = 3, binding = 2) readonly buffer SpotLightBuffer{   

    SpotLight objects[];
} dyn_spot_lights_buffer;
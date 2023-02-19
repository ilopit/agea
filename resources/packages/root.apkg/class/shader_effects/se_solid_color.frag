#version 450


layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inTexCoord;

// Params
layout (set = 0, binding = 0) uniform CameraData 
{
    mat4 projection;
    mat4 view;
    vec3 camPos;
} dyn_camera_data;

layout(set = 0, binding = 1) uniform SceneData{
    vec4 lights[4];
    vec4 lightColor;
    vec4 lightPos;
} dyn_scene_data;


// materials
struct MaterialData
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

//all object matrices
layout(std140, set = 1, binding = 1) readonly buffer MaterialBuffer{   

    MaterialData objects[];
} dyn_material_buffer;


layout(set = 2, binding = 0) uniform sampler2D tex1;


// Constants

layout(push_constant) uniform Constants
{
    uint material_id;
} constants;



layout (location = 0) out vec4 outColor;

void main()
{
    MaterialData material = dyn_material_buffer.objects[constants.material_id];

    // ambient
    vec3 ambient = dyn_scene_data.lightColor.xyz * material.ambient;

    // diffuse 
    vec3 norm = normalize(inNormal);
    vec3 lightDir = normalize(dyn_scene_data.lightPos.xyz - inFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = dyn_scene_data.lightColor.xyz * (diff * material.diffuse);

    // specular
    vec3 viewDir = normalize(dyn_camera_data.camPos - inFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = dyn_scene_data.lightColor.xyz * (spec * material.specular);  

    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, 1.0);
}
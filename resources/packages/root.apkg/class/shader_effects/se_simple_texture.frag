#version 450


layout (location = 0) in vec3 inWorldPos;
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
	float albedo;
	float roughness;
    vec4 color;
	float metallic;
	float gamma;
    vec3 color2;
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
    vec3 object_color = texture(tex1, inTexCoord).xyz;
    outColor = vec4(object_color, 1.0);
}
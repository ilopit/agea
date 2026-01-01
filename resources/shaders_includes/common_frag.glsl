// Input
layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

// Output
layout (location = 0) out vec4 out_color;

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

    uint local_lights_size;
    uint local_light_ids[8];  // slot in respective buffer
} constants;

struct DirLight {
    vec3 direction;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct gpu_universal_light_data
{
    vec3 position;
    vec3 direction;  // unused for point lights
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    uint type;
    float cut_off;
    float outer_cut_off;
    float constant;
    float linear;
    float quadratic;
};

layout(std140, set = 1, binding = 1) readonly buffer DirLightBuffer{   

    DirLight objects[];
} dyn_directional_lights_buffer;

layout(std140, set = 1, binding = 2) readonly buffer LightDataBuffer{   

    gpu_universal_light_data objects[];
} dyn_gpu_universal_light_data;

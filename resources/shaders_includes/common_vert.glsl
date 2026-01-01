layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out vec3 out_world_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_tex_coord;

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

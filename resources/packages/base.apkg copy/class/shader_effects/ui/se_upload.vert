#version 450

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out vec2 out_tex_coord;



out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
    float x = in_tex_coord.x; 
    float y = in_tex_coord.y; 

    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);
    out_tex_coord = vec2(x, y);
}
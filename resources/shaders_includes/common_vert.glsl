#extension GL_GOOGLE_include_directive: enable
#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_object_types.h"
#include "gpu_types/gpu_camera_types.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out vec3 out_world_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_tex_coord;

layout (set = KGPU_global_descriptor_sets, binding = 0) uniform camera_vbo 
{
   camera_data obj;
} dyn_camera_data;


//all object matrices
layout(std140, set = KGPU_objects_descriptor_sets, binding = 0) readonly buffer object_data_buffer{

    object_data objects[];
} dyn_object_buffer;

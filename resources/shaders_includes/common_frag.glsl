#extension GL_GOOGLE_include_directive: enable
#include "gpu_types/gpu_light_types.h"
#include "gpu_types/gpu_camera_types.h"
#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_push_constants.h"

// Input
layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

// Output
layout (location = 0) out vec4 out_color;

// Constants
layout(push_constant) uniform Constants
{   
    push_constants obj;
} constants;

// Bindings
layout (set = KGPU_global_descriptor_sets, binding = 0) uniform camera_vbo 
{
   camera_data obj;
} dyn_camera_data;

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_directional_light_binding) readonly buffer DirLightBuffer{

    directional_light_data objects[];
} dyn_directional_lights_buffer;

layout(std140, set = KGPU_objects_descriptor_sets, binding = KGPU_objects_universal_light_binding) readonly buffer LightDataBuffer{

    universal_light_data objects[];
} dyn_gpu_universal_light_data;

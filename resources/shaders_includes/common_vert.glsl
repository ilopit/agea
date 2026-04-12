// Common vertex shader — I/O layouts + helpers
// Push constants and BDA macros must be declared by including shader BEFORE this file.
// Required macros: dyn_instance_slots, dyn_object_buffer

#include "gpu_types/gpu_generic_constants.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

#ifndef KRYGA_SKINNED
layout (location = 4) in vec2 in_lightmap_uv;
#endif

layout (location = 0) out vec3 out_world_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_tex_coord;

// Lightmap UV always declared for stable vertex interface.
// Non-lightmapped shaders output vec2(0) — the fragment shader ignores it.
layout (location = 4) out vec2 out_lightmap_uv;
layout (location = 5) out flat uint out_object_idx;

uint get_object_index(uint instance_base) {
    return dyn_instance_slots.slots[instance_base + gl_InstanceIndex];
}

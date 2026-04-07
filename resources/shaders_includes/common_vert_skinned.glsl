// Common skinned vertex shader include
// Extends common_vert.glsl with skeletal animation support

#define KRYGA_SKINNED
#include "common_vert.glsl"

// Additional vertex inputs for skinning
// skinned_vertex_data has no uv2, bones start at location 4
layout (location = 4) in uvec4 in_bone_indices;
layout (location = 5) in vec4 in_bone_weights;

// Compute the skinning matrix for the current vertex
// bone_offset comes from object_data, indexes into the global bone SSBO
mat4 get_skin_matrix(uint obj_idx) {
    uint bone_offset = dyn_object_buffer.objects[obj_idx].bone_offset;
    uint bone_count  = dyn_object_buffer.objects[obj_idx].bone_count;

    // Not skinned - return identity
    if (bone_count == 0u) {
        return mat4(1.0);
    }

    mat4 skin_mat =
        in_bone_weights.x * dyn_bone_matrices.matrices[bone_offset + in_bone_indices.x] +
        in_bone_weights.y * dyn_bone_matrices.matrices[bone_offset + in_bone_indices.y] +
        in_bone_weights.z * dyn_bone_matrices.matrices[bone_offset + in_bone_indices.z] +
        in_bone_weights.w * dyn_bone_matrices.matrices[bone_offset + in_bone_indices.w];

    return skin_mat;
}

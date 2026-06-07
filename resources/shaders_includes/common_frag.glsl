// Common fragment shader — aggregator.
//
// Split into per-subsystem modules (included below in dependency order). Every
// shader that includes this file keeps working unchanged; the modules are NOT
// meant to be included directly.
//
// Push constants, BDA extensions, and dyn_X macros must be declared by the
// including shader BEFORE this file.
// Required macros (depending on pass):
//   dyn_camera_data, dyn_object_buffer, dyn_shadow_data,
//   dyn_cluster_config, dyn_cluster_light_counts, dyn_cluster_light_indices,
//   dyn_directional_lights_buffer, dyn_gpu_universal_light_data

#extension GL_EXT_nonuniform_qualifier : require
#include "gpu_types/gpu_generic_constants.h"

// Bindings, I/O, cluster index + object/config getters.
#include "frag_core.glsl"

// Shadow system. Order matters (glslc has no forward declarations):
//   pcf      — sampling primitives used by everything below
//   pcss     — soft-shadow path; uses pcf taps
//   directional — CSM entry point; dispatches to pcf or pcss
//   local    — spot/point; uses pcf dispatch
#include "shadow_pcf.glsl"
#include "shadow_pcss.glsl"
#include "shadow_directional.glsl"
#include "shadow_local.glsl"

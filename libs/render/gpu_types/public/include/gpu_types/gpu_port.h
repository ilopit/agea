#ifndef GPU_PORT_H
#define GPU_PORT_H

#ifdef __cplusplus

#include <glm_unofficial/glm.h>

// Port types
namespace kryga::gpu
{
using vec2 = ::glm::vec2;
using vec3 = ::glm::vec3;
using vec4 = ::glm::vec4;
using mat3 = ::glm::mat3;
using mat4 = ::glm::mat4;
using uint = ::std::uint32_t;
using uint64 = ::std::uint64_t;
using uvec2 = ::glm::uvec2;
using uvec4 = ::glm::uvec4;
using ivec4 = ::glm::ivec4;
}  // namespace kryga::gpu

#define std140_struct struct alignas(16)
#define push_struct struct alignas(4)

#define GPU_BEGIN_NAMESPACE \
    namespace kryga::gpu    \
    {

#define GPU_END_NAMESPACE }

#else  // GLSL

// GLSL handles alignment via layout qualifiers, not per-field
#define std140_struct struct
#define push_struct struct

#define GPU_BEGIN_NAMESPACE
#define GPU_END_NAMESPACE

#endif

// ============================================================================
// Type-aware alignment macros for std140 layout
// These apply correct alignment per type, preventing misuse
// ============================================================================
#ifdef __cplusplus
   // std140 layout rules:
// - float/int/uint: 4-byte alignment
// - vec2: 8-byte alignment
// - vec3/vec4/mat3/mat4: 16-byte alignment
#define std140_float alignas(4) float
#define std140_int alignas(4) int32_t
#define std140_uint alignas(4) uint32_t
#define std140_vec2 alignas(8) vec2
#define std140_vec3 alignas(16) vec3
#define std140_vec4 alignas(16) vec4
#define std140_uvec4 alignas(16) uvec4
#define std140_ivec4 alignas(16) ivec4
#define std140_mat3 alignas(16) mat3
#define std140_mat4 alignas(16) mat4

// std430 layout rules (same as std140 for these types)
#define std430_float alignas(4) float
#define std430_int alignas(4) int32_t
#define std430_uint alignas(4) uint32_t
#define std430_vec2 alignas(8) vec2
#define std430_vec3 alignas(16) vec3
#define std430_vec4 alignas(16) vec4
#define std430_uvec4 alignas(16) uvec4
#define std430_ivec4 alignas(16) ivec4
#define std430_mat3 alignas(16) mat3
#define std430_mat4 alignas(16) mat4

// Push constant layout (scalar alignment)
#define push_float alignas(4) float
#define push_int alignas(4) int32_t
#define push_uint alignas(4) uint32_t
#define push_uint64 alignas(8) uint64_t
#define push_uvec2 alignas(4) uvec2
#define push_vec2 alignas(4) vec2
#define push_vec3 alignas(4) vec3
#define push_vec4 alignas(4) vec4
#define push_mat3 alignas(4) mat3
#define push_mat4 alignas(4) mat4
#else
// GLSL: Plain types (alignment handled by layout qualifier)
#define std140_float float
#define std140_int int
#define std140_uint uint
#define std140_vec2 vec2
#define std140_vec3 vec3
#define std140_vec4 vec4
#define std140_uvec4 uvec4
#define std140_ivec4 ivec4
#define std140_mat3 mat3
#define std140_mat4 mat4

#define std430_float float
#define std430_int int
#define std430_uint uint
#define std430_vec2 vec2
#define std430_vec3 vec3
#define std430_vec4 vec4
#define std430_uvec4 uvec4
#define std430_ivec4 ivec4
#define std430_mat3 mat3
#define std430_mat4 mat4

#define push_float float
#define push_int int
#define push_uint uint
#define push_uint64 uint64_t
#define push_uvec2 uvec2
#define push_vec2 vec2
#define push_vec3 vec3
#define push_vec4 vec4
#define push_mat3 mat3
#define push_mat4 mat4
#endif

#endif  // GPU_PORT_H

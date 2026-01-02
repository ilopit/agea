#ifndef GPU_PORT_H
#define GPU_PORT_H

#ifdef __cplusplus

#include <glm_unofficial/glm.h>

// Port types
namespace agea::gpu
{
using vec2 = ::glm::vec2;
using vec3 = ::glm::vec3;
using vec4 = ::glm::vec4;
using mat3 = ::glm::mat3;
using mat4 = ::glm::mat4;
using uint = ::std::uint32_t;
}  // namespace agea::gpu

// std140 layout requires vec3 to be aligned to 16 bytes
#define align_std140 alignas(16)
#define align_pc alignas(4)

// Struct with 16-byte alignment (for std140 array stride)
#define gpu_struct_std140 struct align_std140
#define gpu_struct_pc struct align_pc

#define GPU_BEGIN_NAMESPACE \
    namespace agea::gpu     \
    {

#define GPU_END_NAMESPACE }

#else

// GLSL handles alignment via layout qualifiers, not per-field
#define align_std140
#define align_pc
// std140 handles struct alignment automatically
#define gpu_struct_std140 struct
#define gpu_struct_pc struct

#define GPU_BEGIN_NAMESPACE
#define GPU_END_NAMESPACE

#endif
#endif

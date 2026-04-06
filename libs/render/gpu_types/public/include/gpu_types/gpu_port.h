#ifndef GPU_PORT_H
#define GPU_PORT_H

#ifdef __cplusplus

#include <glm_unofficial/glm.h>

// Port types — scalar layout, natural C++ alignment matches GPU
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

#define GPU_BEGIN_NAMESPACE \
    namespace kryga::gpu    \
    {

#define GPU_END_NAMESPACE }

#else  // GLSL

#define GPU_BEGIN_NAMESPACE
#define GPU_END_NAMESPACE

#endif

#endif  // GPU_PORT_H

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

// BDA address as uvec2 - avoids shaderInt64 requirement on mobile
// alignas(8) matches GLSL std430 uvec2 alignment (vec2 of uint is 8-byte aligned)
struct alignas(8) bda_addr
{
    uint32_t lo;
    uint32_t hi;

    bda_addr() = default;
    constexpr bda_addr(uint32_t low, uint32_t high) : lo(low), hi(high) {}
};

inline bda_addr
make_bda_addr(uint64_t addr)
{
    return bda_addr(uint32_t(addr), uint32_t(addr >> 32));
}
using ivec4 = ::glm::ivec4;
}  // namespace kryga::gpu

#define GPU_BEGIN_NAMESPACE \
    namespace kryga::gpu    \
    {

#define GPU_END_NAMESPACE }

#else  // GLSL

#define GPU_BEGIN_NAMESPACE
#define GPU_END_NAMESPACE

// BDA address as uvec2 - avoids shaderInt64 requirement on mobile
#define bda_addr uvec2

#endif

#endif  // GPU_PORT_H

#pragma once

#include <utils/kryga_enum.h>

#include <cstdint>
#include <string_view>

namespace kryga
{
namespace core
{

// `unknown` (0xff) is a unique value, so it stays in the table — to_string and
// from_string handle it like any other. The `first`/`last` iteration bounds are
// aliases of real entries (would trip the duplicate-value check), so they live
// as constants beside the enum — see SENTINEL note in kryga_enum.h.
// clang-format off
#define KRG_ARCHITYPE_LIST(X)                  \
    X(smart_object,  0,    "smart_object")     \
    X(game_object,   1,    "game_object")      \
    X(component,     2,    "component")         \
    X(mesh,          3,    "mesh")             \
    X(texture,       4,    "texture")          \
    X(shader_effect, 5,    "shader_effect")    \
    X(material,      6,    "material")         \
    X(sampler,       7,    "sampler")          \
    X(unknown,       0xff, "unknown")
// clang-format on

KRG_declare_enum_simple(architype, uint8_t, KRG_ARCHITYPE_LIST)

inline constexpr architype architype_first = architype::smart_object;
inline constexpr architype architype_last = architype::sampler;

}  // namespace core
}  // namespace kryga

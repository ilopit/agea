#pragma once

#include <stdint.h>

namespace agea
{
namespace core
{
enum class architype : uint8_t
{
    unknown = 0xff,
    first = 0,
    smart_object = first,
    game_object = 1,
    component = 2,
    mesh = 3,
    texture = 4,
    shader_effect = 5,
    material = 6,
    last = material
};
}
}  // namespace agea
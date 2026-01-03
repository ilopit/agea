#pragma once

#include <cstdint>

#include <string_view>

namespace kryga
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

constexpr std::string_view
to_string(architype a)
{
    switch (a)
    {
    case kryga::core::architype::smart_object:
        return std::string_view("smart_object");
    case kryga::core::architype::game_object:
        return std::string_view("game_object");
    case kryga::core::architype::component:
        return std::string_view("component");
    case kryga::core::architype::mesh:
        return std::string_view("mesh");
    case kryga::core::architype::texture:
        return std::string_view("texture");
    case kryga::core::architype::shader_effect:
        return std::string_view("shader_effect");
    case kryga::core::architype::material:
        return std::string_view("material");
    default:
        return std::string_view("unknown");
    }
}

}  // namespace core
}  // namespace kryga
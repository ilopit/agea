#pragma once

#include "vfx/emitter.h"

#include <cstdint>

namespace kryga
{
namespace vfx
{

enum class preset : std::uint8_t
{
    particles,
    smoke,
    dust
};

emitter_params
make_preset(preset p);

const char*
preset_name(preset p);

constexpr std::uint8_t preset_count = 3;

}  // namespace vfx
}  // namespace kryga

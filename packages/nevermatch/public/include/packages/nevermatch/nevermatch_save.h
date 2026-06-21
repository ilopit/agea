#pragma once

#include <cstdint>

namespace kryga::nevermatch
{

// Game-specific run state owned by nevermatch_mode and persisted through the mode's
// save()/load() hooks. Plain struct with explicit serialization — see the note in
// nevermatch_mode.cpp on why this is not a reflected blob.
struct nevermatch_save
{
    int64_t gold = 0;
    float playtime = 0.f;
};

}  // namespace kryga::nevermatch

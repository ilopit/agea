#pragma once

#include <serialization/serialization_fwds.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace kryga::game
{

// Persistent, cross-level player data owned by the game_session. It outlives any
// single level (a level is unloaded on switch; this is not), which is what makes
// it the right place to carry progress across a level transition or a save slot.
//
// First cut: a deliberately small, game-agnostic payload plus a generic flags map
// for progress bits. Binding these fields onto the level's actual player object is
// game-specific and intentionally left to the caller (see game_session notes).
struct player_state
{
    // Level id (utils::id::str()) the player is currently in — restored on load.
    std::string current_level;

    // Generic progress flags / counters, addressed by name.
    std::unordered_map<std::string, int64_t> flags;

    void
    to_container(serialization::container& c) const;

    bool
    from_container(const serialization::container& c);
};

}  // namespace kryga::game

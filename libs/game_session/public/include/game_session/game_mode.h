#pragma once

#include <serialization/serialization_fwds.h>

#include <utils/id.h>

#include <functional>
#include <memory>

namespace kryga::game
{

class game_session;

// The pluggable, game-specific half of the play lifecycle. A game lives in its own
// package and provides ONE game_mode subclass; the engine-owned game_session is the
// stable host that delegates to it. All hooks default to no-ops so a game overrides
// only what it needs. The dependency is one-way: a game package depends on this
// interface; game_session never knows the concrete game.
//
// Lifecycle ordering (driven by game_session):
//   on_start        - once, when play begins (after begin_play() broadcast). Set up
//                     run-scoped state here; it persists across level switches.
//   on_level_loaded - every time a level becomes active (initial + each switch).
//   on_tick         - every play frame, after the level tick + quest poll.
//   on_stop         - once, when play ends (before the end_play() broadcast).
//   save / load     - serialize game-specific state into the save-slot container.
class game_mode
{
public:
    virtual ~game_mode() = default;

    virtual void
    on_start(game_session&)
    {
    }

    virtual void
    on_stop(game_session&)
    {
    }

    virtual void
    on_tick(game_session&, float /*dt*/)
    {
    }

    virtual void
    on_level_loaded(game_session&, const utils::id& /*level_id*/)
    {
    }

    // Persist / restore game-specific state. The container is the "mode" sub-node of
    // the save slot; the mode owns its schema. (The engine's reflected serialization
    // is smart_object/vfs-centric and does not serialize a standalone blob into an
    // arbitrary node, so the mode writes its state through this hook.)
    virtual void
    save(serialization::container&)
    {
    }

    virtual void
    load(const serialization::container&)
    {
    }
};

// A game package registers its factory once, at static-init (before the session's
// on_init runs). If nothing registers, the session uses the no-op base game_mode.
// Last registration wins.
void
register_game_mode(std::function<std::unique_ptr<game_mode>()> factory);

// Used by game_session::on_init to build the active mode.
std::unique_ptr<game_mode>
create_registered_game_mode();

}  // namespace kryga::game

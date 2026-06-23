#pragma once

#include <game_session/game_mode.h>

#include "packages/nevermatch/nevermatch_save.h"

namespace kryga::core
{
struct io_context;
}

namespace kryga::nevermatch
{

// The global-flow extension axis: this game's single game_mode. The engine-owned
// game_session delegates the play lifecycle here. Registered with the session via
// register_game_mode() in package.nevermatch.cpp.
class nevermatch_mode : public game::game_mode
{
public:
    void
    on_start(game::game_session& s) override;

    void
    on_stop(game::game_session& s) override;

    void
    on_tick(game::game_session& s, float dt) override;

    void
    on_level_loaded(game::game_session& s, const utils::id& level_id) override;

    void
    save(serialization::container& c) override;

    void
    load(const serialization::container& c) override;

private:
    // Input entry point for the game. Registered against the "mouse_pressed" action
    // (mouse_button_left, see resources/configs/inputs.acfg) on play-start and torn
    // down on play-stop — the in/out bracket of the session. The click coordinate
    // arrives on the event (captured at press), so it stays correct once input is
    // queued. This is where match-cube picking will be driven from.
    void
    on_select_pressed(const core::io_context& e);

    nevermatch_save m_save;
};

}  // namespace kryga::nevermatch

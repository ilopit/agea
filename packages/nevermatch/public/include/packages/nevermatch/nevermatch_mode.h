#pragma once

#include <game_session/game_mode.h>

#include "packages/nevermatch/nevermatch_save.h"

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
    nevermatch_save m_save;
};

}  // namespace kryga::nevermatch

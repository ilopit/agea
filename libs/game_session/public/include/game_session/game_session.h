#pragma once

#include <game_session/player_state.h>
#include <game_session/quest_log.h>

#include <global_state/system.h>

#include <utils/id.h>

#include <optional>
#include <span>
#include <string_view>

namespace kryga::game
{

enum class session_state
{
    idle = 0,
    playing
};

// Owns the running-game lifecycle, tier-agnostic. In play mode the engine main
// loop routes the per-frame tick THROUGH this system instead of ticking the level
// directly (and instead of the editor). The editor keeps only editor-specific
// concerns (snapshot/rollback, camera save); begin_play/end_play live here.
class game_session : public gs::system
{
public:
    std::string_view
    name() const override
    {
        return "game_session";
    }

    std::span<const std::string_view>
    deps() const override;

    // Play lifecycle. Idempotent w.r.t. m_state so editor F5, game-tier load and
    // level switching can all drive it without double begin_play/end_play.
    void
    start();  // begin_play() on every level game_object; state -> playing

    void
    stop();  // end_play() on every level game_object; state -> idle

    bool
    is_playing() const
    {
        return m_state == session_state::playing;
    }

    // Per-frame; the main loop calls this only while playing. Ticks the level and
    // polls quests. The level switch itself is NOT applied here — the engine drains
    // it via take_pending_level() (see below) so teardown never runs mid-iteration
    // and the session avoids a back-dependency on the engine target.
    void
    tick(float dt);

    // Game code requests a switch; it is deferred. The engine owns load_level (it
    // does the render-resource teardown), so the engine drains the request, calls
    // stop() -> load_level -> start() around it. Keeps the dependency one-way:
    // engine -> game_session, never the reverse.
    void
    request_switch_level(const utils::id& level_id);

    // Engine-facing: returns and clears any pending switch target. Returns
    // std::nullopt when there is nothing to switch to.
    std::optional<utils::id>
    take_pending_level();

    // Player-state persistence to rtcache://saves/slot{slot}.yaml.
    bool
    save(int slot);

    bool
    load(int slot);

    player_state&
    player()
    {
        return m_player;
    }

    quest_log&
    quests()
    {
        return m_quests;
    }

private:
    session_state m_state = session_state::idle;
    std::optional<utils::id> m_pending_level;
    player_state m_player;  // persists across level switches (not owned by a level)
    quest_log m_quests;
};

}  // namespace kryga::game

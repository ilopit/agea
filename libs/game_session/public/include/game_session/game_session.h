#pragma once

#include <game_session/game_mode.h>
#include <game_session/player_state.h>
#include <game_session/quest_log.h>

#include <global_state/system.h>

#include <utils/id.h>

#include <memory>
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

// Owns the running-game lifecycle, tier-agnostic. In play mode the engine main loop
// routes the per-frame tick THROUGH this system instead of ticking the level directly
// (and instead of the editor). The session is a STABLE HOST: game-specific behavior is
// injected via a game_mode (composition, not inheritance) registered by a game package.
//
// Two lifecycle axes, deliberately separate so a game_mode's run-scoped state survives
// level switches:
//   * play session boundary - enter_play() / exit_play() -> mode.on_start / on_stop
//   * level activation       - on_level_will_change() / on_level_changed() (each switch)
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

    // Builds the active game_mode from the package-registered factory (or a no-op base).
    void
    on_init(gs::state&) override;

    // --- play session boundary (editor F5, or game-tier first level load) ---

    void
    enter_play();  // begin_play() broadcast -> mode.on_start -> mode.on_level_loaded

    void
    exit_play();  // mode.on_stop -> end_play() broadcast -> idle

    bool
    is_playing() const
    {
        return m_state == session_state::playing;
    }

    // --- level activation (engine brackets load_level with these on a switch) ---

    // Old level still loaded: end_play() its objects. No mode lifetime change.
    void
    on_level_will_change();

    // New level loaded: begin_play() its objects + mode.on_level_loaded.
    void
    on_level_changed();

    // Per play frame: level tick + quest poll + mode.on_tick.
    void
    tick(float dt);

    // Game code requests a switch; deferred. The engine owns load_level (render-resource
    // teardown), so it drains the request and brackets load_level with the level hooks.
    void
    request_switch_level(const utils::id& level_id);

    std::optional<utils::id>
    take_pending_level();

    // Player + game-mode state persistence to rtcache://saves/slot{slot}.yaml.
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

    game_mode&
    mode()
    {
        return *m_mode;
    }

    // Simple session score (HUD-facing). Persists for the play session; game code
    // drives it, the UI reads it. Not tied to a level.
    int64_t
    get_score() const
    {
        return m_score;
    }
    void
    add_score(int64_t delta)
    {
        m_score += delta;
    }
    void
    set_score(int64_t v)
    {
        m_score = v;
    }

private:
    // begin_play()/end_play() broadcast to every game_object in the current level.
    void
    broadcast_begin_play();

    void
    broadcast_end_play();

    session_state m_state = session_state::idle;
    std::optional<utils::id> m_pending_level;
    player_state m_player;  // persists across level switches (not owned by a level)
    quest_log m_quests;
    int64_t m_score = 0;
    std::unique_ptr<game_mode> m_mode;  // the pluggable game-specific behavior
};

}  // namespace kryga::game

#pragma once

#include <game/game_system.h>

#include <utils/id.h>

#include <functional>
#include <string>
#include <vector>

namespace kryga
{

namespace core
{
class level;
}
namespace root
{
class game_object;
}

namespace game
{

enum class session_state
{
    inactive,
    loading,
    playing,
    paused,
    game_over
};

class game_session : public game_system
{
public:
    std::string_view
    name() const override
    {
        return "game_session";
    }

    game_phase
    phase() const override
    {
        return game_phase::pre_physics;
    }

    void
    on_begin_play() override;

    void
    on_end_play() override;

    void
    tick(float dt) override;

    // --- Level transitions ---

    void
    request_level_load(const utils::id& level_id);

    const utils::id&
    get_current_level_id() const
    {
        return m_current_level_id;
    }

    // --- Player access ---

    root::game_object*
    get_player() const
    {
        return m_player;
    }

    void
    set_player(root::game_object* p)
    {
        m_player = p;
    }

    // --- State ---

    session_state
    get_state() const
    {
        return m_state;
    }

    float
    get_play_time() const
    {
        return m_play_time;
    }

    // --- Callbacks for game-specific reactions ---

    using state_callback = std::function<void(session_state old_state, session_state new_state)>;

    void
    on_state_changed(state_callback cb)
    {
        m_state_callbacks.push_back(std::move(cb));
    }

private:
    void
    transition_to(session_state new_state);

    session_state m_state = session_state::inactive;
    root::game_object* m_player = nullptr;
    utils::id m_current_level_id;
    utils::id m_pending_level_id;
    float m_play_time = 0.f;

    std::vector<state_callback> m_state_callbacks;
};

}  // namespace game
}  // namespace kryga

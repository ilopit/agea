#include "game/game_session.h"

#include <global_state/global_state.h>

#include <core/model_system.h>
#include <core/level.h>

#include <utils/kryga_log.h>

namespace kryga::game
{

void
game_session::on_begin_play()
{
    m_play_time = 0.f;

    auto* lvl = glob::glob_state().getr_model().current_level;
    if (lvl)
    {
        m_current_level_id = lvl->get_id();
    }

    transition_to(session_state::playing);

    ALOG_INFO("game_session: begin play (level: {})", m_current_level_id.cstr());
}

void
game_session::on_end_play()
{
    m_player = nullptr;
    m_play_time = 0.f;
    m_pending_level_id = {};

    transition_to(session_state::inactive);

    ALOG_INFO("game_session: end play");
}

void
game_session::tick(float dt)
{
    if (m_state != session_state::playing)
    {
        return;
    }

    m_play_time += dt;
}

void
game_session::request_level_load(const utils::id& level_id)
{
    m_pending_level_id = level_id;
    transition_to(session_state::loading);

    ALOG_INFO("game_session: level load requested — {}", level_id.cstr());
}

void
game_session::transition_to(session_state new_state)
{
    if (m_state == new_state)
    {
        return;
    }

    auto old = m_state;
    m_state = new_state;

    for (auto& cb : m_state_callbacks)
    {
        cb(old, new_state);
    }
}

}  // namespace kryga::game

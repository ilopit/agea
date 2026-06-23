#include <game_session/game_session.h>

#include <global_state/global_state.h>

#include <core/level.h>
#include <core/model_system.h>

#include <packages/root/model/game_object.h>

#include <serialization/serialization.h>

#include <vfs/rid.h>
#include <utils/kryga_log.h>

#include <string>

namespace kryga::game
{

namespace
{
vfs::rid
slot_rid(int slot)
{
    return {"rtcache", std::string("saves/slot") + std::to_string(slot) + ".yaml"};
}
}  // namespace

std::span<const std::string_view>
game_session::deps() const
{
    static constexpr std::string_view d[] = {"model"};
    return d;
}

void
game_session::on_init(gs::state&)
{
    m_mode = create_registered_game_mode();
}

void
game_session::broadcast_begin_play()
{
    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return;
    }
    for (auto& [id, obj] : lvl->get_game_objects().get_items())
    {
        if (auto go = obj->as<root::game_object>())
        {
            go->begin_play();
        }
    }
    m_player.current_level = lvl->get_id().str();
}

void
game_session::broadcast_end_play()
{
    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return;
    }
    for (auto& [id, obj] : lvl->get_game_objects().get_items())
    {
        if (auto go = obj->as<root::game_object>())
        {
            go->end_play();
        }
    }
}

void
game_session::enter_play()
{
    if (m_state == session_state::playing)
    {
        return;
    }
    m_state = session_state::playing;

    broadcast_begin_play();
    m_mode->on_start(*this);

    if (auto* lvl = glob::glob_state().getr_model().current_level)
    {
        m_mode->on_level_loaded(*this, lvl->get_id());
    }
}

void
game_session::exit_play()
{
    if (m_state == session_state::idle)
    {
        return;
    }
    m_mode->on_stop(*this);
    broadcast_end_play();
    m_state = session_state::idle;
}

void
game_session::on_level_will_change()
{
    if (m_state == session_state::playing)
    {
        broadcast_end_play();
    }
}

void
game_session::on_level_changed()
{
    if (m_state != session_state::playing)
    {
        return;
    }
    broadcast_begin_play();
    if (auto* lvl = glob::glob_state().getr_model().current_level)
    {
        m_mode->on_level_loaded(*this, lvl->get_id());
    }
}

void
game_session::tick(float dt)
{
    if (auto* lvl = glob::glob_state().getr_model().current_level)
    {
        lvl->tick(dt);
    }
    m_quests.poll(dt);
    m_mode->on_tick(*this, dt);
}

void
game_session::request_switch_level(const utils::id& level_id)
{
    m_pending_level = level_id;
}

std::optional<utils::id>
game_session::take_pending_level()
{
    auto v = m_pending_level;
    m_pending_level.reset();
    return v;
}

bool
game_session::save(int slot)
{
    serialization::container c;

    serialization::container player_c;
    m_player.to_container(player_c);
    c["player"] = player_c;

    serialization::container quests_c;
    m_quests.to_container(quests_c);
    c["quests"] = quests_c;

    serialization::container mode_c;
    m_mode->save(mode_c);
    c["mode"] = mode_c;

    const bool ok = serialization::write_container(slot_rid(slot), c);
    if (!ok)
    {
        ALOG_ERROR("game_session: failed to save slot {}", slot);
    }
    return ok;
}

bool
game_session::load(int slot)
{
    serialization::container c;
    if (!serialization::read_container(slot_rid(slot), c))
    {
        ALOG_WARN("game_session: no save in slot {}", slot);
        return false;
    }

    if (c["player"])
    {
        m_player.from_container(c["player"]);
    }
    if (c["quests"])
    {
        m_quests.from_container(c["quests"]);
    }
    if (c["mode"])
    {
        m_mode->load(c["mode"]);
    }
    return true;
}

}  // namespace kryga::game

#include "packages/nevermatch/nevermatch_mode.h"

#include <game_session/game_session.h>

#include <serialization/serialization.h>

#include <utils/kryga_log.h>

namespace kryga::nevermatch
{

void
nevermatch_mode::on_start(game::game_session&)
{
    ALOG_INFO("nevermatch_mode: play started (gold={})", m_save.gold);
}

void
nevermatch_mode::on_stop(game::game_session&)
{
    ALOG_INFO("nevermatch_mode: play stopped (playtime={:.1f}s)", m_save.playtime);
}

void
nevermatch_mode::on_tick(game::game_session& s, float dt)
{
    m_save.playtime += dt;

    // Global game flow lives here. For example, a real game would drive transitions:
    //   if (boss_defeated) s.request_switch_level(AID("victory"));
    // Left inert in the template so it never switches to a non-existent level.
    (void)s;
}

void
nevermatch_mode::on_level_loaded(game::game_session&, const utils::id& level_id)
{
    ALOG_INFO("nevermatch_mode: level loaded '{}'", level_id.str());
}

void
nevermatch_mode::save(serialization::container& c)
{
    c["gold"] = m_save.gold;
    c["playtime"] = m_save.playtime;
}

void
nevermatch_mode::load(const serialization::container& c)
{
    if (c["gold"])
    {
        m_save.gold = c["gold"].as<int64_t>();
    }
    if (c["playtime"])
    {
        m_save.playtime = c["playtime"].as<float>();
    }
}

}  // namespace kryga::nevermatch

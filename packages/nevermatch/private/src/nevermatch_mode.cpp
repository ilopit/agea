#include "packages/nevermatch/nevermatch_mode.h"

#include <game_session/game_session.h>

#include <picking/picking.h>

#include <packages/nevermatch/model/nevermatch_player.h>

#include <packages/root/model/components/camera_component.h>
#include <packages/root/model/game_object.h>

#include <core/input_provider.h>
#include <core/level.h>
#include <core/model_system.h>

#include <serialization/serialization.h>

#include <utils/kryga_log.h>

namespace kryga::nevermatch
{

void
nevermatch_mode::on_start(game::game_session&)
{
    ALOG_INFO("nevermatch_mode: play started (gold={})", m_save.gold);

    if (auto* ip = glob::get_input_provider())
    {
        ip->register_fixed_action(
            AID("mouse_pressed"), true, this, &nevermatch_mode::on_select_pressed);
    }

    // The game owns its player. Spawned here (not by the engine/editor) so root
    // stays generic. on_start runs after the begin_play() broadcast, so the freshly
    // spawned player is begin_play()'d explicitly. The active camera's aspect is
    // driven per-frame by the engine's update_cameras(); we only set fov/near/far.
    if (auto* lvl = glob::glob_state().getr_model().current_level)
    {
        nevermatch_player::construct_params pp;
        if (auto* player = lvl->spawn_object<nevermatch_player>(AID("player_0"), pp))
        {
            player->begin_play();
            if (auto* cam = player->get_camera())
            {
                cam->set_active_camera(true);
                cam->set_perspective(60.f, 1.f, 0.1f, 2000.f);
            }
        }
    }
}

void
nevermatch_mode::on_stop(game::game_session&)
{
    if (auto* ip = glob::get_input_provider())
    {
        ip->unregister_owner(this);
    }

    ALOG_INFO("nevermatch_mode: play stopped (playtime={:.1f}s)", m_save.playtime);
}

void
nevermatch_mode::on_select_pressed(const core::io_context& e)
{
    auto* cam = picking::find_active_camera();
    if (!cam)
    {
        ALOG_WARN("nevermatch_mode: click ignored — no active camera in level");
        return;
    }

    auto* hit = picking::pick_object_under_cursor(*cam, e.current.mouse_x, e.current.mouse_y);

    ALOG_INFO("nevermatch_mode: pick at ({}, {}) -> '{}'",
              e.current.mouse_x,
              e.current.mouse_y,
              hit ? hit->get_id().str() : std::string("<none>"));
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

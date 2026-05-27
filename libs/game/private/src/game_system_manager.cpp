#include "game/game_system_manager.h"

#include <global_state/global_state.h>

#include <utils/kryga_log.h>

namespace kryga
{

void
state_mutator__game_system_manager::set(gs::state& s)
{
    s.m_game_system_manager = s.create_box<game::game_system_manager>("game_system_manager");
}

namespace game
{

void
game_system_manager::register_system(std::unique_ptr<game_system> sys)
{
    auto phase_idx = static_cast<size_t>(sys->phase());
    KRG_check(phase_idx < k_phase_count, "Invalid game_phase");

    ALOG_INFO("game_system_manager: registered '{}' (phase {})", sys->name(), phase_idx);

    m_by_phase[phase_idx].push_back(sys.get());
    m_systems.push_back(std::move(sys));
}

void
game_system_manager::tick_phase(game_phase phase, float dt)
{
    auto phase_idx = static_cast<size_t>(phase);
    for (auto* sys : m_by_phase[phase_idx])
    {
        if (sys->is_enabled())
        {
            sys->tick(dt);
        }
    }
}

void
game_system_manager::on_begin_play()
{
    for (auto& sys : m_systems)
    {
        sys->on_begin_play();
    }
}

void
game_system_manager::on_end_play()
{
    for (auto& sys : m_systems)
    {
        sys->on_end_play();
    }
}

}  // namespace game
}  // namespace kryga

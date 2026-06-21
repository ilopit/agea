#include <game_session/game_session.h>

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__game_session::set(gs::state& s)
{
    auto p = s.create_box<game::game_session>("game_session");
    s.m_game_session = p;
    s.register_system(p);
}

}  // namespace kryga

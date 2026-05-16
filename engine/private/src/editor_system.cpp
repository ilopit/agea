#include <engine/editor_system.h>

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__editor_system::set(gs::state& s)
{
    auto p = s.create_box<engine::editor_system>("editor_system");
    s.m_editor_system = p;
    s.register_system(p);
}

}  // namespace kryga

#include <vulkan_render/render_system.h>

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__render::set(gs::state& s)
{
    auto p = s.create_box<render::render_system>("render_system");
    s.m_render = p;
    s.register_system(p);
}

}  // namespace kryga

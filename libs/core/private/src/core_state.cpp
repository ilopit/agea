#include <core/core_state.h>

#include <global_state/global_state.h>

#include <core/model_system.h>
#include <core/subsystem_queues.h>
#include <core/reflection/lua_api.h>

namespace kryga::core
{

void
state_mutator__model::set(gs::state& es)
{
    es.m_model = es.create_box<core::model_system>("model_system");
    es.register_system(es.m_model);
}

void
state_mutator__lua_api::set(gs::state& es)
{
    es.m_lua = es.create_box<reflection::lua_api>("lua_api");
}

}  // namespace kryga::core

namespace kryga
{

void
state_mutator__subsystem_queues::set(gs::state& s)
{
    // Neutral model->subsystem command boundary — its own box, not on any system, so
    // producers enqueue here instead of reaching into a subsystem's system object.
    auto p = s.create_box<subsystem_queues>("subsystem_queues");
    s.m_subsystem_queues = p;
}

}  // namespace kryga

#include <core/core_state.h>

#include <global_state/global_state.h>

#include <core/model_system.h>
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

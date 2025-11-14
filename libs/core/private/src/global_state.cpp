#include "core/global_state.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/lua_api.h>
#include <core/caches/cache_set.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <core/level.h>
#include <core/package.h>
#include <core/id_generator.h>
#include <resource_locator/resource_locator.h>

namespace agea::glob
{

::agea::core::state&
glob_state()
{
    static ::agea::core::state g_glob_state;
    return g_glob_state;
}
void
glob_state_reset()
{
    glob_state() = {};
}
}  // namespace agea::glob

namespace agea::core
{

state::state()
{
}

int
state::schedule_action(state_stage stage, scheduled_action action)
{
    auto& node = m_scheduled_actions[(size_t)stage];

    node.push_back(action);
    std::cerr << std::format("Scheduling action state {:x}  at {}, total {} \n", (int)this,
                             (size_t)stage, node.size());
    return (int)node.size();
}

void
state::run_create()
{
    AGEA_check(m_stage == state_stage::create, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::connect;
}

void
state::run_connect()
{
    AGEA_check(m_stage == state_stage::connect, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::init;
}

void
state::run_init()
{
    AGEA_check(m_stage == state_stage::init, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::ready;
}

void
state_mutator__caches::set(state& es)
{
    auto class_cache = es.create_box<cache_set>();
    auto instance_cache = es.create_box<cache_set>();

    es.m_class_set = class_cache;

    es.m_class_objects_cache = &class_cache->objects;
    es.m_class_components_cache = &class_cache->components;
    es.m_class_game_objects_cache = &class_cache->game_objects;
    es.m_class_materials_cache = &class_cache->materials;
    es.m_class_meshes_cache = &class_cache->meshes;
    es.m_class_textures_cache = &class_cache->textures;
    es.m_class_shader_effects_cache = &class_cache->shader_effects;
    es.m_class_cache_map = &class_cache->map;

    es.m_instance_set = instance_cache;

    es.m_instance_objects_cache = &instance_cache->objects;
    es.m_instance_components_cache = &instance_cache->components;
    es.m_instance_game_objects_cache = &instance_cache->game_objects;
    es.m_instance_materials_cache = &instance_cache->materials;
    es.m_instance_meshes_cache = &instance_cache->meshes;
    es.m_instance_textures_cache = &instance_cache->textures;
    es.m_instance_shader_effects_cache = &instance_cache->shader_effects;
    es.m_instance_cache_map = &instance_cache->map;
}

void
state_mutator__package_manager::set(state& es)
{
    es.m_pm = es.create_box<package_manager>();
}

void
state_mutator__level_manager::set(state& es)
{
    es.m_lm = es.create_box<level_manager>();
}

void
state_mutator__reflection_manager::set(state& es)
{
    es.m_rm = es.create_box<reflection::reflection_type_registry>();
}

void
state_mutator__lua_api::set(state& es)
{
    es.m_lua = es.create_box<reflection::lua_api>();
}

void
state_mutator__id_generator::set(state& es)
{
    es.m_id_generator = es.create_box<core::id_generator>();
}

void
state_mutator__current_level::set(level& lvl, state& es)
{
    es.m_current_level = &lvl;
}

void
state_mutator__resource_locator::set(state& es)
{
    es.m_resource_locator = es.create_box<resource_locator>();
}

void
state::run_items(state_stage stage)
{
    auto& node = m_scheduled_actions[(size_t)stage];
    for (auto& a : node)
    {
        a(*this);
    }

    node.clear();
}

}  // namespace agea::core
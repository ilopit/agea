#pragma once

#include <utils/singleton_instance.h>
#include <utils/defines_utils.h>

#include <memory>
#include <functional>
#include <array>

#define AGEA_gen_getter(n, t)                           \
    t* get_##n() const                                  \
    {                                                   \
        return m_##n;                                   \
    }                                                   \
    t& getr_##n() const                                 \
    {                                                   \
        AGEA_check(m_##n, "Instance should be alive!"); \
        return *m_##n;                                  \
    }

namespace agea
{

namespace core
{
class caches_map;
class cache_set;
class cache_set_ref;
class class_objects_cache;
class object_load_context;
class level;
class package;
class static_package;
class package_manager;
class level_manager;
class object_mapping;
class id_generator;

struct objects_cache;
struct components_cache;
struct game_objects_cache;
struct textures_cache;
struct meshes_cache;
struct materials_cache;
struct shader_effects_cache;

struct state_mutator__caches;
struct state_mutator__level_manager;
struct state_mutator__package_manager;
struct state_mutator__id_generator;
struct state_mutator__reflection_manager;
struct state_mutator__lua_api;
struct state_mutator__current_level;

};  // namespace core

namespace reflection
{
class reflection_type_registry;
class lua_api;

}  // namespace reflection

class resource_locator;

namespace gs
{

struct state_base_box
{
    virtual ~state_base_box()
    {
    }
};

template <typename T>
struct state_type_box : public state_base_box
{
    state_type_box() = default;

    template <typename... Args>
    state_type_box(Args... args)
        : m_data(std::forward<Args>(args)...)
    {
    }

    T*
    get()
    {
        return &m_data;
    }

    T m_data;
};
class state;

using scheduled_action = std::function<void(state& s)>;

class state
{
    friend class core::state_mutator__caches;
    friend class core::state_mutator__current_level;
    friend class core::state_mutator__id_generator;
    friend class core::state_mutator__level_manager;
    friend class core::state_mutator__lua_api;
    friend class core::state_mutator__package_manager;
    friend class core::state_mutator__reflection_manager;
    friend class state_mutator__resource_locator;

public:
    enum class state_stage
    {
        create = 0,
        connect,
        init,
        ready,
        number_of_stages
    };

    state();

    int
    schedule_action(state_stage execute_at, scheduled_action action);

    void
    run_create();

    void
    run_connect();

    void
    run_init();

    AGEA_gen_getter(class_set, core::cache_set);
    AGEA_gen_getter(class_objects_cache, core::objects_cache);
    AGEA_gen_getter(class_components_cache, core::components_cache);
    AGEA_gen_getter(class_game_objects_cache, core::game_objects_cache);
    AGEA_gen_getter(class_materials_cache, core::materials_cache);
    AGEA_gen_getter(class_meshes_cache, core::meshes_cache);
    AGEA_gen_getter(class_textures_cache, core::textures_cache);
    AGEA_gen_getter(class_shader_effects_cache, core::shader_effects_cache);
    AGEA_gen_getter(class_cache_map, core::caches_map);

    AGEA_gen_getter(instance_set, core::cache_set);
    AGEA_gen_getter(instance_objects_cache, core::objects_cache);
    AGEA_gen_getter(instance_components_cache, core::components_cache);
    AGEA_gen_getter(instance_game_objects_cache, core::game_objects_cache);
    AGEA_gen_getter(instance_materials_cache, core::materials_cache);
    AGEA_gen_getter(instance_meshes_cache, core::meshes_cache);
    AGEA_gen_getter(instance_textures_cache, core::textures_cache);
    AGEA_gen_getter(instance_shader_effects_cache, core::shader_effects_cache);
    AGEA_gen_getter(instance_cache_map, core::caches_map);

    AGEA_gen_getter(current_level, core::level);
    AGEA_gen_getter(lm, core::level_manager);
    AGEA_gen_getter(pm, core::package_manager);
    AGEA_gen_getter(lua, reflection::lua_api);
    AGEA_gen_getter(rm, reflection::reflection_type_registry);
    AGEA_gen_getter(id_generator, core::id_generator);
    AGEA_gen_getter(resource_locator, resource_locator);

    template <typename T>
    T*
    create_box()
    {
        state_type_box<T>* box = new state_type_box<T>();
        auto obj = box->get();

        std::unique_ptr<state_base_box> ubox(box);

        m_boxes.emplace_back(std::move(ubox));

        return obj;
    }

private:
    void
    run_items(state_stage stage);

    // clang-format off

    // Caches
    core::cache_set*            m_class_set = nullptr;
    core::objects_cache*        m_class_objects_cache = nullptr;
    core::components_cache*     m_class_components_cache = nullptr;
    core::game_objects_cache*   m_class_game_objects_cache = nullptr;
    core::materials_cache*      m_class_materials_cache = nullptr;
    core::meshes_cache*         m_class_meshes_cache = nullptr;
    core::textures_cache*       m_class_textures_cache = nullptr;
    core::shader_effects_cache* m_class_shader_effects_cache = nullptr;
    core::caches_map*           m_class_cache_map = nullptr;

    core::cache_set*            m_instance_set = nullptr;
    core::objects_cache*        m_instance_objects_cache = nullptr;
    core::components_cache*     m_instance_components_cache = nullptr;
    core::game_objects_cache*   m_instance_game_objects_cache = nullptr;
    core::materials_cache*      m_instance_materials_cache = nullptr;
    core::meshes_cache*         m_instance_meshes_cache = nullptr;
    core::textures_cache*       m_instance_textures_cache = nullptr;
    core::shader_effects_cache* m_instance_shader_effects_cache = nullptr;
    core::caches_map*           m_instance_cache_map = nullptr;

    // Managers
    core::level*                    m_current_level = nullptr;
    core::level_manager*            m_lm = nullptr;
    core::package_manager*          m_pm = nullptr;
    reflection::lua_api*            m_lua = nullptr;
    reflection::reflection_type_registry* m_rm = nullptr;
    core::id_generator*             m_id_generator = nullptr;
    resource_locator*               m_resource_locator = nullptr;

    // clang-format on

    std::vector<std::unique_ptr<state_base_box>> m_boxes;

    std::array<std::vector<scheduled_action>, (size_t)state_stage::number_of_stages>
        m_scheduled_actions;

    state_stage m_stage = state_stage::create;
};

}  // namespace gs

namespace glob
{

::agea::gs::state&
glob_state();

void
glob_state_reset();
}  // namespace glob
}  // namespace agea

#define AGEA_gen__static_schedule(when, action)          \
    const int AGEA_concat2(si_identifier, __COUNTER__) = \
        agea::glob::glob_state().schedule_action(when, action)

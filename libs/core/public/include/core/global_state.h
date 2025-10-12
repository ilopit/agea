#pragma once

#include <core/model_fwds.h>
#include <utils/singleton_instance.h>
#include <utils/defines_utils.h>

#include <memory>
#include <functional>
#include <array>

namespace agea
{
namespace reflection
{
class reflection_type_registry;
class lua_api;

}  // namespace reflection
}  // namespace agea

namespace agea::core
{

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

struct state_mutator__caches
{
    static void
    set(state& es);
};

struct state_mutator__level_manager
{
    static void
    set(state& es);
};

struct state_mutator__package_manager
{
    static void
    set(state& es);
};

struct state_mutator__id_generator
{
    static void
    set(state& es);
};

struct state_mutator__reflection_manager
{
    static void
    set(state& es);
};

struct state_mutator__lua_api
{
    static void
    set(state& es);
};

struct state_mutator__current_level
{
    static void
    set(level& lvl, state& es);
};

using scheduled_action = std::function<void(state& s)>;

class state
{
    friend class state_mutator__caches;
    friend class state_mutator__current_level;
    friend class state_mutator__id_generator;
    friend class state_mutator__level_manager;
    friend class state_mutator__lua_api;
    friend class state_mutator__package_manager;
    friend class state_mutator__reflection_manager;

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

    AGEA_gen_getter(class_set, cache_set);
    AGEA_gen_getter(class_objects_cache, objects_cache);
    AGEA_gen_getter(class_components_cache, components_cache);
    AGEA_gen_getter(class_game_objects_cache, game_objects_cache);
    AGEA_gen_getter(class_materials_cache, materials_cache);
    AGEA_gen_getter(class_meshes_cache, meshes_cache);
    AGEA_gen_getter(class_textures_cache, textures_cache);
    AGEA_gen_getter(class_shader_effects_cache, shader_effects_cache);
    AGEA_gen_getter(class_cache_map, caches_map);

    AGEA_gen_getter(instance_set, cache_set);
    AGEA_gen_getter(instance_objects_cache, objects_cache);
    AGEA_gen_getter(instance_components_cache, components_cache);
    AGEA_gen_getter(instance_game_objects_cache, game_objects_cache);
    AGEA_gen_getter(instance_materials_cache, materials_cache);
    AGEA_gen_getter(instance_meshes_cache, meshes_cache);
    AGEA_gen_getter(instance_textures_cache, textures_cache);
    AGEA_gen_getter(instance_shader_effects_cache, shader_effects_cache);
    AGEA_gen_getter(instance_cache_map, caches_map);

    AGEA_gen_getter(current_level, level);
    AGEA_gen_getter(lm, level_manager);
    AGEA_gen_getter(pm, package_manager);
    AGEA_gen_getter(lua, reflection::lua_api);
    AGEA_gen_getter(rm, reflection::reflection_type_registry);
    AGEA_gen_getter(id_generator, id_generator);

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
    cache_set*            m_class_set = nullptr;
    objects_cache*        m_class_objects_cache = nullptr;
    components_cache*     m_class_components_cache = nullptr;
    game_objects_cache*   m_class_game_objects_cache = nullptr;
    materials_cache*      m_class_materials_cache = nullptr;
    meshes_cache*         m_class_meshes_cache = nullptr;
    textures_cache*       m_class_textures_cache = nullptr;
    shader_effects_cache* m_class_shader_effects_cache = nullptr;
    caches_map*           m_class_cache_map = nullptr;

    cache_set*            m_instance_set = nullptr;
    objects_cache*        m_instance_objects_cache = nullptr;
    components_cache*     m_instance_components_cache = nullptr;
    game_objects_cache*   m_instance_game_objects_cache = nullptr;
    materials_cache*      m_instance_materials_cache = nullptr;
    meshes_cache*         m_instance_meshes_cache = nullptr;
    textures_cache*       m_instance_textures_cache = nullptr;
    shader_effects_cache* m_instance_shader_effects_cache = nullptr;
    caches_map*           m_instance_cache_map = nullptr;

    // Managers
    level*                    m_current_level = nullptr;
    level_manager*            m_lm = nullptr;
    package_manager*          m_pm = nullptr;
    reflection::lua_api*      m_lua = nullptr;
    reflection::reflection_type_registry* m_rm = nullptr;
    id_generator* m_id_generator = nullptr;

    // clang-format on

    std::vector<std::unique_ptr<state_base_box>> m_boxes;

    std::array<std::vector<scheduled_action>, (size_t)state_stage::number_of_stages>
        m_scheduled_actions;

    state_stage m_stage = state_stage::create;
};

}  // namespace agea::core

namespace agea::glob
{
struct state : public simple_singletone<::agea::core::state>
{
};
}  // namespace agea::glob

#define AGEA_gen__static_schedule(when, action)                 \
    const static int AGEA_concat2(si_identifier, __COUNTER__) = \
        ::agea::glob::state::getr().schedule_action(when, action)

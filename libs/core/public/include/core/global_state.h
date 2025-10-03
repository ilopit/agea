#pragma once

#include <core/model_fwds.h>
#include <utils/singleton_instance.h>
#include <utils/defines_utils.h>

#include <memory>
#include <functional>

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

struct state_caches_mutator
{
    static void
    set(state& es);
};

struct state_level_mutator
{
    static void
    set(state& es);
};

struct state_package_mutator
{
    static void
    set(state& es);
};

struct state_reflection_manager_mutator
{
    static void
    set(state& es);
};

struct state_lua_mutator
{
    static void
    set(state& es);
};

struct state_current_level_mutator
{
    static void
    set(level& lvl, state& es);
};

using create_action = std::function<void(state& s)>;
using register_action = std::function<void(state& s)>;
using init_action = std::function<void(state& s)>;

class state
{
    friend class state_caches_mutator;
    friend class state_level_mutator;
    friend class state_package_mutator;
    friend class state_reflection_manager_mutator;
    friend class state_lua_mutator;
    friend class state_current_level_mutator;

public:
    enum class stage_stage
    {
        create = 0,
        regs,
        init,
        ready
    };

    state();

    int
    schedule_create(create_action action);

    int
    schedule_register(register_action action);

    int
    schedule_init(init_action action);

    void
    run_create();

    void
    run_register();

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

    // clang-format on

    std::vector<std::unique_ptr<state_base_box>> m_boxes;

    std::vector<create_action> m_create_actions;
    std::vector<register_action> m_register_actions;
    std::vector<init_action> m_init_actions;

    stage_stage m_stage = stage_stage::create;
};

}  // namespace agea::core

namespace agea::glob
{
struct state : public simple_singletone<::agea::core::state>
{
};
}  // namespace agea::glob

#define AGEA_schedule_static_create(action)                     \
    const static int AGEA_concat2(si_identifier, __COUNTER__) = \
        ::agea::glob::state::getr().schedule_create(action)

#define AGEA_schedule_static_register(action)                   \
    const static int AGEA_concat2(si_identifier, __COUNTER__) = \
        ::agea::glob::state::getr().schedule_register(action)

#define AGEA_schedule_static_init(action)                       \
    const static int AGEA_concat2(si_identifier, __COUNTER__) = \
        ::agea::glob::state::getr().schedule_init(action)
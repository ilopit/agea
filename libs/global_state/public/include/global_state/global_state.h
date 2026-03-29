#pragma once

#include <utils/singleton_instance.h>
#include <utils/defines_utils.h>
#include <utils/check.h>

#include <memory>
#include <functional>
#include <array>
#include <string>

#define KRG_gen_getter(n, t)                           \
    t* get_##n() const                                 \
    {                                                  \
        return m_##n;                                  \
    }                                                  \
    t& getr_##n() const                                \
    {                                                  \
        KRG_check(m_##n, "Instance should be alive!"); \
        return *m_##n;                                 \
    }

namespace kryga
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
class queues;

struct objects_cache;
struct components_cache;
struct game_objects_cache;
struct textures_cache;
struct meshes_cache;
struct materials_cache;
struct samplers_cache;
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

class vulkan_engine;

namespace vfs
{
class virtual_file_system;
}  // namespace vfs
class render_bridge;
class native_window;
struct engine_counters;

namespace render
{
class vulkan_render;
class vulkan_render_loader;
class render_device;
}  // namespace render

namespace animation
{
class animation_system;
}  // namespace animation

namespace engine
{
class game_editor;
class input_manager;
}  // namespace engine

namespace ui
{
class ui;
}

namespace editor
{
class config;
}

struct state_mutator__vfs;
struct state_mutator__engine;
struct state_mutator__game_editor;
struct state_mutator__input_manager;
struct state_mutator__config;
struct state_mutator__render_device;
struct state_mutator__vulkan_render_loader;
struct state_mutator__ui;
struct state_mutator__native_window;
struct state_mutator__vulkan_render;
struct state_mutator__engine_counters;
struct state_mutator__queues;
struct state_mutator__render_bridge;
struct state_mutator__animation_system;

namespace gs
{

struct state_base_box
{
    state_base_box(std::string name);
    virtual ~state_base_box();

    std::string box_name;
};

template <typename T>
struct state_type_box : public state_base_box
{
    state_type_box() = default;

    template <typename... Args>
    state_type_box(std::string name, Args... args)
        : state_base_box(std::move(name))
        , m_data(std::forward<Args>(args)...)
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
    friend class ::kryga::state_mutator__vfs;

    friend class ::kryga::state_mutator__engine;
    friend class ::kryga::state_mutator__game_editor;
    friend class ::kryga::state_mutator__input_manager;
    friend class ::kryga::state_mutator__config;
    friend class ::kryga::state_mutator__render_device;
    friend class ::kryga::state_mutator__vulkan_render_loader;
    friend class ::kryga::state_mutator__ui;
    friend class ::kryga::state_mutator__native_window;
    friend class ::kryga::state_mutator__vulkan_render;
    friend class ::kryga::state_mutator__engine_counters;
    friend class ::kryga::state_mutator__queues;
    friend class ::kryga::state_mutator__render_bridge;
    friend class ::kryga::state_mutator__animation_system;

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
    ~state();

    state(const state&) = delete;
    state&
    operator=(const state&) = delete;
    state(state&&) = default;

    state&
    operator=(state&& other);

    int
    schedule_action(state_stage execute_at, scheduled_action action);

    void
    run_create();

    void
    run_connect();

    void
    run_init();

    KRG_gen_getter(class_set, core::cache_set);
    KRG_gen_getter(class_objects_cache, core::objects_cache);
    KRG_gen_getter(class_components_cache, core::components_cache);
    KRG_gen_getter(class_game_objects_cache, core::game_objects_cache);
    KRG_gen_getter(class_materials_cache, core::materials_cache);
    KRG_gen_getter(class_samplers_cache, core::samplers_cache);
    KRG_gen_getter(class_meshes_cache, core::meshes_cache);
    KRG_gen_getter(class_textures_cache, core::textures_cache);
    KRG_gen_getter(class_shader_effects_cache, core::shader_effects_cache);
    KRG_gen_getter(class_cache_map, core::caches_map);

    KRG_gen_getter(instance_set, core::cache_set);
    KRG_gen_getter(instance_objects_cache, core::objects_cache);
    KRG_gen_getter(instance_components_cache, core::components_cache);
    KRG_gen_getter(instance_game_objects_cache, core::game_objects_cache);
    KRG_gen_getter(instance_materials_cache, core::materials_cache);
    KRG_gen_getter(instance_samplers_cache, core::samplers_cache);
    KRG_gen_getter(instance_meshes_cache, core::meshes_cache);
    KRG_gen_getter(instance_textures_cache, core::textures_cache);
    KRG_gen_getter(instance_shader_effects_cache, core::shader_effects_cache);
    KRG_gen_getter(instance_cache_map, core::caches_map);

    KRG_gen_getter(current_level, core::level);
    KRG_gen_getter(lm, core::level_manager);
    KRG_gen_getter(pm, core::package_manager);
    KRG_gen_getter(lua, reflection::lua_api);
    KRG_gen_getter(rm, reflection::reflection_type_registry);
    KRG_gen_getter(id_generator, core::id_generator);
    KRG_gen_getter(vfs, vfs::virtual_file_system);

    KRG_gen_getter(engine, vulkan_engine);
    KRG_gen_getter(game_editor, engine::game_editor);
    KRG_gen_getter(input_manager, engine::input_manager);
    KRG_gen_getter(config, editor::config);
    KRG_gen_getter(render_device, render::render_device);
    KRG_gen_getter(vulkan_render_loader, render::vulkan_render_loader);
    KRG_gen_getter(ui, ui::ui);
    KRG_gen_getter(native_window, native_window);
    KRG_gen_getter(vulkan_render, render::vulkan_render);
    KRG_gen_getter(engine_counters, engine_counters);
    KRG_gen_getter(queues, core::queues);
    KRG_gen_getter(render_bridge, render_bridge);
    KRG_gen_getter(animation_system, animation::animation_system);

    template <typename T>
    T*
    create_box(std::string name)
    {
        state_type_box<T>* box = new state_type_box<T>(std::move(name));
        auto obj = box->get();

        std::unique_ptr<state_base_box> ubox(box);

        m_boxes.emplace_back(std::move(ubox));

        return obj;
    }

    template <typename T>
    T*
    create_box_with_obj(std::string name, T value)
    {
        state_type_box<T>* box = new state_type_box<T>(std::move(name), std::move(value));
        auto obj = box->get();

        std::unique_ptr<state_base_box> ubox(box);

        m_boxes.emplace_back(std::move(ubox));

        return obj;
    }

private:
    void
    run_items(state_stage stage);

    void
    cleanup();

    // clang-format off

    // Caches
    core::cache_set*            m_class_set = nullptr;
    core::objects_cache*        m_class_objects_cache = nullptr;
    core::components_cache*     m_class_components_cache = nullptr;
    core::game_objects_cache*   m_class_game_objects_cache = nullptr;
    core::materials_cache*      m_class_materials_cache = nullptr;
    core::samplers_cache*       m_class_samplers_cache = nullptr;
    core::meshes_cache*         m_class_meshes_cache = nullptr;
    core::textures_cache*       m_class_textures_cache = nullptr;
    core::shader_effects_cache* m_class_shader_effects_cache = nullptr;
    core::caches_map*           m_class_cache_map = nullptr;

    core::cache_set*            m_instance_set = nullptr;
    core::objects_cache*        m_instance_objects_cache = nullptr;
    core::components_cache*     m_instance_components_cache = nullptr;
    core::game_objects_cache*   m_instance_game_objects_cache = nullptr;
    core::materials_cache*      m_instance_materials_cache = nullptr;
    core::samplers_cache*       m_instance_samplers_cache = nullptr;
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
    vfs::virtual_file_system*       m_vfs = nullptr;

    // Engine singletons
    vulkan_engine*                  m_engine = nullptr;
    engine::game_editor*            m_game_editor = nullptr;
    engine::input_manager*          m_input_manager = nullptr;
    editor::config*                 m_config = nullptr;
    render::render_device*          m_render_device = nullptr;
    render::vulkan_render_loader*   m_vulkan_render_loader = nullptr;
    ui::ui*                         m_ui = nullptr;
    native_window*                  m_native_window = nullptr;
    render::vulkan_render*          m_vulkan_render = nullptr;
    engine_counters*                m_engine_counters = nullptr;
    core::queues*                   m_queues = nullptr;
    render_bridge*                  m_render_bridge = nullptr;
    animation::animation_system*    m_animation_system = nullptr;

    // clang-format on

    std::vector<std::unique_ptr<state_base_box>> m_boxes;

    std::array<std::vector<scheduled_action>, (size_t)state_stage::number_of_stages>
        m_scheduled_actions;

    state_stage m_stage = state_stage::create;
};

}  // namespace gs

// Full definitions for engine/render/native/bridge mutators
// (forward-declared above for friend access)

struct state_mutator__engine
{
    static void
    set(vulkan_engine* e, gs::state& s)
    {
        s.m_engine = e;
    }
};

struct state_mutator__game_editor
{
    static void
    set(gs::state& s);
};

struct state_mutator__input_manager
{
    static void
    set(gs::state& s);
};

struct state_mutator__config
{
    static void
    set(gs::state& s);
};

struct state_mutator__render_device
{
    static void
    set(gs::state& s);
};

struct state_mutator__vulkan_render_loader
{
    static void
    set(gs::state& s);
};

struct state_mutator__ui
{
    static void
    set(gs::state& s);
};

struct state_mutator__native_window
{
    static void
    set(gs::state& s);
};

struct state_mutator__vulkan_render
{
    static void
    set(gs::state& s);
};

struct state_mutator__engine_counters
{
    static void
    set(gs::state& s);
};

struct state_mutator__queues
{
    static void
    set(gs::state& s);
};

struct state_mutator__render_bridge
{
    static void
    set(gs::state& s);
};

struct state_mutator__animation_system
{
    static void
    set(gs::state& s);
};

namespace glob
{

::kryga::gs::state&
glob_state();

void
glob_state_reset();
}  // namespace glob
}  // namespace kryga

#define KRG_gen__static_schedule(when, action)          \
    const int KRG_concat2(si_identifier, __COUNTER__) = \
        kryga::glob::glob_state().schedule_action(when, action)

#pragma once

#include <global_state/system.h>

#include <utils/singleton_instance.h>
#include <utils/defines_utils.h>
#include <utils/check.h>

#include <memory>
#include <functional>
#include <array>
#include <string>
#include <vector>
#include <algorithm>

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
class model_system;
class queues;

struct state_mutator__model;
struct state_mutator__lua_api;

};  // namespace core

namespace reflection
{
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
class render_system;
}  // namespace render

namespace animation
{
class animation_system;
}  // namespace animation

namespace engine
{
class game_editor;
class input_manager;
class editor_system;
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
struct state_mutator__editor_system;
struct state_mutator__input_manager;
struct state_mutator__config;
struct state_mutator__render;
struct state_mutator__native_window;
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
    friend class core::state_mutator__model;
    friend class core::state_mutator__lua_api;
    friend class ::kryga::state_mutator__vfs;

    friend class ::kryga::state_mutator__engine;
    friend class ::kryga::state_mutator__editor_system;
    friend class ::kryga::state_mutator__input_manager;
    friend class ::kryga::state_mutator__config;
    friend class ::kryga::state_mutator__render;
    friend class ::kryga::state_mutator__native_window;
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

    core::model_system*
    get_model() const
    {
        return m_model;
    }

    core::model_system&
    getr_model() const
    {
        KRG_check(m_model, "Instance should be alive!");
        return *m_model;
    }

    render::render_system*
    get_render() const
    {
        return m_render;
    }

    render::render_system&
    getr_render() const
    {
        KRG_check(m_render, "Instance should be alive!");
        return *m_render;
    }

    KRG_gen_getter(lua, reflection::lua_api);
    KRG_gen_getter(vfs, vfs::virtual_file_system);

    KRG_gen_getter(engine, vulkan_engine);
    KRG_gen_getter(editor_system, engine::editor_system);
    KRG_gen_getter(input_manager, engine::input_manager);
    KRG_gen_getter(config, editor::config);
    KRG_gen_getter(native_window, native_window);
    KRG_gen_getter(engine_counters, engine_counters);
    KRG_gen_getter(queues, core::queues);
    KRG_gen_getter(render_bridge, render_bridge);

    KRG_gen_getter(animation_system, animation::animation_system);

    void register_system(system* sys);

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

    // Model system (caches, managers, reflection)
    core::model_system*             m_model = nullptr;

    // Render system (device, renderer, loader)
    render::render_system*          m_render = nullptr;

    // Subsystems
    reflection::lua_api*            m_lua = nullptr;
    vfs::virtual_file_system*       m_vfs = nullptr;

    // Engine singletons
    vulkan_engine*                  m_engine = nullptr;
    engine::editor_system*          m_editor_system = nullptr;
    engine::input_manager*          m_input_manager = nullptr;
    editor::config*                 m_config = nullptr;
    native_window*                  m_native_window = nullptr;
    engine_counters*                m_engine_counters = nullptr;
    core::queues*                   m_queues = nullptr;
    render_bridge*                  m_render_bridge = nullptr;
    animation::animation_system*    m_animation_system = nullptr;

    // clang-format on

    std::vector<std::unique_ptr<state_base_box>> m_boxes;
    std::vector<system*> m_systems;

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

struct state_mutator__editor_system
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

struct state_mutator__render
{
    static void
    set(gs::state& s);
};

struct state_mutator__native_window
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

// Process-lifetime registry for schedule actions added via static initializers
// (KRG_gen__static_schedule). Unlike gs::state::m_scheduled_actions — which gets
// cleared on glob_state_reset — these persist across resets so hot reload and
// tests that re-init the engine actually re-run package registrations etc.
//
// Internally: keyed by state_stage; each stage's vector is replayed at the start
// of the corresponding gs::state::run_X() call.
struct persistent_schedule
{
    static int
    add(gs::state::state_stage stage, gs::scheduled_action action);

    static void
    run(gs::state::state_stage stage, gs::state& s);
};
}  // namespace glob
}  // namespace kryga

#define KRG_gen__static_schedule(when, action)          \
    const int KRG_concat2(si_identifier, __COUNTER__) = \
        kryga::glob::persistent_schedule::add(when, action)

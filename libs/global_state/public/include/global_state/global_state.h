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

// Systems
namespace core
{
class model_system;
}
namespace render
{
class render_system;
}
namespace animation
{
class animation_system;
}
namespace physics
{
class physics_system;
}
namespace audio
{
class audio_system;
}
namespace game
{
class game_system_manager;
}
namespace engine
{
class editor_system;
}
namespace vfs
{
class virtual_file_system;
}

// Services
namespace engine
{
class input_manager;
}
namespace reflection
{
class lua_api;
}
class render_translator;
class audio_bridge;
struct engine_counters;
struct subsystem_queues;

// Singletons
class vulkan_engine;
class native_window;
namespace editor
{
class config;
}

// Mutator forward declarations
namespace core
{
struct state_mutator__model;
struct state_mutator__lua_api;
}  // namespace core

// Systems
struct state_mutator__render;
struct state_mutator__animation_system;
struct state_mutator__physics_system;
struct state_mutator__audio_system;
struct state_mutator__game_system_manager;
struct state_mutator__editor_system;
struct state_mutator__vfs;

// Services
struct state_mutator__input_manager;
struct state_mutator__render_translator;
struct state_mutator__audio_bridge;
struct state_mutator__engine_counters;
struct state_mutator__subsystem_queues;

// Singletons
struct state_mutator__engine;
struct state_mutator__native_window;
struct state_mutator__config;

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
    // Systems
    friend class core::state_mutator__model;
    friend class ::kryga::state_mutator__render;
    friend class ::kryga::state_mutator__animation_system;
    friend class ::kryga::state_mutator__editor_system;
    friend class ::kryga::state_mutator__vfs;

    // Services
    friend class ::kryga::state_mutator__input_manager;
    friend class core::state_mutator__lua_api;
    friend class ::kryga::state_mutator__render_translator;
    friend class ::kryga::state_mutator__audio_bridge;
    friend class ::kryga::state_mutator__engine_counters;
    friend class ::kryga::state_mutator__subsystem_queues;
    friend class ::kryga::state_mutator__physics_system;
    friend class ::kryga::state_mutator__game_system_manager;
    friend class ::kryga::state_mutator__audio_system;

    // Singletons
    friend class ::kryga::state_mutator__engine;
    friend class ::kryga::state_mutator__native_window;
    friend class ::kryga::state_mutator__config;

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

    // Systems
    KRG_gen_getter(model, core::model_system);
    KRG_gen_getter(render, render::render_system);
    KRG_gen_getter(animation_system, animation::animation_system);
    KRG_gen_getter(editor_system, engine::editor_system);
    KRG_gen_getter(vfs, vfs::virtual_file_system);

    // Services
    KRG_gen_getter(input_manager, engine::input_manager);
    KRG_gen_getter(lua, reflection::lua_api);
    KRG_gen_getter(render_translator, render_translator);
    KRG_gen_getter(audio_bridge, audio_bridge);
    KRG_gen_getter(engine_counters, engine_counters);
    KRG_gen_getter(subsystem_queues, subsystem_queues);

    // Singletons
    KRG_gen_getter(engine, vulkan_engine);
    KRG_gen_getter(native_window, native_window);
    KRG_gen_getter(config, editor::config);

    void
    register_system(system* sys);

    KRG_gen_getter(physics_system, physics::physics_system);
    KRG_gen_getter(game_system_manager, game::game_system_manager);
    KRG_gen_getter(audio_system, audio::audio_system);

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

    // Systems
    core::model_system*             m_model = nullptr;
    render::render_system*          m_render = nullptr;
    animation::animation_system*    m_animation_system = nullptr;
    engine::editor_system*          m_editor_system = nullptr;
    vfs::virtual_file_system*       m_vfs = nullptr;

    // Services
    engine::input_manager*          m_input_manager = nullptr;
    reflection::lua_api*            m_lua = nullptr;
    render_translator*                  m_render_translator = nullptr;
    audio_bridge*                   m_audio_bridge = nullptr;
    engine_counters*                m_engine_counters = nullptr;
    subsystem_queues*               m_subsystem_queues = nullptr;
    physics::physics_system*        m_physics_system = nullptr;
    game::game_system_manager*      m_game_system_manager = nullptr;
    audio::audio_system*            m_audio_system = nullptr;

    // Singletons
    vulkan_engine*                  m_engine = nullptr;
    native_window*                  m_native_window = nullptr;
    editor::config*                 m_config = nullptr;

    // clang-format on

    std::vector<std::unique_ptr<state_base_box>> m_boxes;
    std::vector<system*> m_systems;

    std::array<std::vector<scheduled_action>, (size_t)state_stage::number_of_stages>
        m_scheduled_actions;

    state_stage m_stage = state_stage::create;
};

}  // namespace gs

// Systems

struct state_mutator__render
{
    static void
    set(gs::state& s);
};

struct state_mutator__animation_system
{
    static void
    set(gs::state& s);
};

struct state_mutator__editor_system
{
    static void
    set(gs::state& s);
};

// Services

struct state_mutator__input_manager
{
    static void
    set(gs::state& s);
};

struct state_mutator__render_translator
{
    static void
    set(gs::state& s);
};

struct state_mutator__audio_bridge
{
    static void
    set(gs::state& s);
};

struct state_mutator__engine_counters
{
    static void
    set(gs::state& s);
};

struct state_mutator__subsystem_queues
{
    static void
    set(gs::state& s);
};

// Singletons

struct state_mutator__engine
{
    static void
    set(vulkan_engine* e, gs::state& s)
    {
        s.m_engine = e;
    }
};

struct state_mutator__native_window
{
    static void
    set(gs::state& s);
};

struct state_mutator__config
{
    static void
    set(gs::state& s);
};

struct state_mutator__physics_system
{
    static void
    set(gs::state& s);
};

struct state_mutator__game_system_manager
{
    static void
    set(gs::state& s);
};

struct state_mutator__audio_system
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

#pragma once

#include <core/model_fwds.h>
#include <core/caches/line_cache.h>
#include <core/input_provider.h>

#include <utils/id.h>
#include <utils/path.h>
#include <utils/check.h>
#include <utils/generic_event_handler.h>

#include <SDL_events.h>

#include <unordered_map>
#include <unordered_set>

union SDL_Event;

namespace kryga
{
namespace engine
{

using core::input_event_id;

struct event_state;

enum class input_event_type
{
    nan = 0,

    press,
    release,
    scale,
    count
};

struct input_action_descriptor;

struct input_scaled_action_handler : utils::generic_event_handler<void, float>
{
    float basic_amp = 1.f;
};

struct input_fixed_action_handler : utils::generic_event_handler<void>
{
};

struct input_action_descriptor
{
    input_event_id id;
    float amp = 0.f;
};

struct input_action
{
    utils::id id;

    std::unordered_map<input_event_id, input_action_descriptor> m_triggers;
};

struct mouse_state
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t xrel = 0;
    int32_t yrel = 0;
};

struct mouse_wheel_state
{
    int32_t x = 0;
    int32_t y = 0;
};

class input_manager : public core::input_provider
{
public:
    input_manager();

    template <typename real_obj, typename real_method>
    bool
    register_scaled_action(const utils::id& id, real_obj* o, real_method m)
    {
        input_scaled_action_handler ev;
        ev.assign(o, m);

        auto itr = m_input_actions.find(id);
        if (itr == m_input_actions.end())
        {
            return false;
        }

        for (auto& t : itr->second.m_triggers)
        {
            ev.basic_amp = t.second.amp;
            m_events_state[t.second.id].m_registered_scaled_handlers.push_back(ev);
        }
        return true;
    }

    template <typename real_obj, typename real_method>
    bool
    register_fixed_action(const utils::id& id, bool pressed, real_obj* o, real_method m)
    {
        input_fixed_action_handler ev;
        ev.assign(o, m);

        auto itr = m_input_actions.find(id);
        if (itr == m_input_actions.end())
        {
            return false;
        }

        if (pressed)
        {
            for (auto& t : itr->second.m_triggers)
            {
                m_events_state[t.second.id].m_registered_pres_fixed_handlers.push_back(ev);
            }
        }
        else
        {
            for (auto& t : itr->second.m_triggers)
            {
                m_events_state[t.second.id].m_registered_release_fixed_handlers.push_back(ev);
            }
        }

        return true;
    }

    bool
    input_tick(float dt);

    bool
    load_actions(const utils::path& path);

    void
    fire_input_event();

    bool
    get_input_state(input_event_id id) override
    {
        return m_events_state[id].is_active;
    }

    void
    unregister_owner(void* owner) override;

    const mouse_state&
    get_mouse_state()
    {
        return m_mouse_axis_state;
    }

protected:
    bool
    do_register_scaled(const utils::id& id, core::input_scaled_handler_data handler) override;

    bool
    do_register_fixed(const utils::id& id,
                      bool pressed,
                      core::input_fixed_handler_data handler) override;

    void
    drop_fired_event();

    void
    consume_sdl_events(const SDL_Event& e);

    mouse_state m_mouse_axis_state;
    mouse_wheel_state m_mouse_wheel_state;

    float m_dur_seconds = 0.f;

    struct event_state
    {
        bool to_drop = false;
        bool is_active = false;
        bool fire_on_start = false;
        float extra_ampl = 1.f;

        void
        reset()
        {
            to_drop = false;
            is_active = false;
            fire_on_start = false;
            extra_ampl = 1.f;
        }

        std::vector<input_fixed_action_handler> m_registered_pres_fixed_handlers;
        std::vector<input_fixed_action_handler> m_registered_release_fixed_handlers;

        std::vector<input_scaled_action_handler> m_registered_scaled_handlers;
    };

    std::unordered_map<input_event_id, event_state> m_events_state;

    std::unordered_map<utils::id, input_action> m_input_actions;

    std::unordered_set<event_state*> m_active_events;
    std::vector<event_state*> m_to_drop_events;

    bool m_updated = true;
};

}  // namespace engine
}  // namespace kryga

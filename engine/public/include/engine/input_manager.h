#pragma once

#include <core/model_fwds.h>
#include <core/caches/line_cache.h>

#include <utils/id.h>
#include <utils/path.h>
#include <utils/singleton_instance.h>
#include <utils/generic_event_handler.h>

#include <SDL_events.h>

#include <unordered_map>
#include <unordered_set>

union SDL_Event;

namespace agea
{
namespace engine
{

struct event_state;

enum class input_event_type
{
    nan = 0,

    press,
    release,
    scale,
    count
};

enum input_event_id
{
    nan = 0,

    mouse_left,
    mouse_wheel,
    mouse_right,

    mouse_move_wheel,
    mouse_move_x,
    mouse_move_y,

    keyboard_return,
    keyboard_escape,
    keyboard_backspace,
    keyboard_tab,
    keyboard_space,
    keyboard_capslock,

    keyboard_0,
    keyboard_1,
    keyboard_2,
    keyboard_3,
    keyboard_4,
    keyboard_5,
    keyboard_6,
    keyboard_7,
    keyboard_8,
    keyboard_9,

    keyboard_a,
    keyboard_b,
    keyboard_c,
    keyboard_d,
    keyboard_e,
    keyboard_f,
    keyboard_g,
    keyboard_h,
    keyboard_i,
    keyboard_j,
    keyboard_k,
    keyboard_l,
    keyboard_m,
    keyboard_n,
    keyboard_o,
    keyboard_p,
    keyboard_q,
    keyboard_r,
    keyboard_s,
    keyboard_t,
    keyboard_u,
    keyboard_v,
    keyboard_w,
    keyboard_x,
    keyboard_y,
    keyboard_z,

    keyboard_f1,
    keyboard_f2,
    keyboard_f3,
    keyboard_f4,
    keyboard_f5,
    keyboard_f6,
    keyboard_f7,
    keyboard_f8,
    keyboard_f9,
    keyboard_f10,
    keyboard_f11,
    keyboard_f12,

    keyboard_printscreen,
    keyboard_scrolllock,
    keyboard_pause,
    keyboard_insert,
    keyboard_home,
    keyboard_pageup,

    keyboard_end,
    keyboard_pagedown,
    keyboard_right,
    keyboard_left,
    keyboard_down,
    keyboard_up,

    keyboard_delete
};

struct input_action_descriptior;

struct input_scaled_action_handler : utils::generic_event_handler<void, float>
{
    float basic_amp = 1.f;
};

struct input_fixed_action_handler : utils::generic_event_handler<void>
{
};

struct input_action_descriptior
{
    input_event_id id;
    float amp = 0.f;
};

struct input_action
{
    utils::id id;

    std::unordered_map<input_event_id, input_action_descriptior> m_triggers;
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

class input_manager
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
    get_input_state(input_event_id id)
    {
        return m_events_state[id].is_active;
    }

    const mouse_state&
    get_mouse_state()
    {
        return m_mouse_axis_state;
    }

protected:
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
        float extran_ampl = 1.f;

        void
        reset()
        {
            to_drop = false;
            is_active = false;
            fire_on_start = false;
            extran_ampl = 1.f;
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

namespace glob
{
struct input_manager : public singleton_instance<::agea::engine::input_manager, input_manager>
{
};
}  // namespace glob
}  // namespace agea
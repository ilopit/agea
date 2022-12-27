#pragma once

#include <model/model_fwds.h>
#include <model/caches/line_cache.h>

#include <utils/id.h>
#include <utils/path.h>
#include <utils/singleton_instance.h>
#include <utils/generic_event_handler.h>

#include <SDL_events.h>

#include <unordered_map>

union SDL_Event;

namespace agea
{
namespace engine
{
enum input_event_type
{
    input_event__nan = 0,
    input_event_press,
    input_event_release,
    input_event_scale,
    input_event_count
};

enum input_event_id
{
    ieid_nan = 0,

    ieid_ms_btm_left,
    ieid_ms_btm_wheel,
    ieid_ms_btm_right,

    ieid_ms_wheel,
    ieid_ms_move_x,
    ieid_ms_move_y,

    ieid_kb_btn_return,
    ieid_kb_btn_escape,
    ieid_kb_btn_backspace,
    ieid_kb_btn_tab,
    ieid_kb_btn_space,
    ieid_kb_btn_capslock,

    ieid_kb_btn_0,
    ieid_kb_btn_1,
    ieid_kb_btn_2,
    ieid_kb_btn_3,
    ieid_kb_btn_4,
    ieid_kb_btn_5,
    ieid_kb_btn_6,
    ieid_kb_btn_7,
    ieid_kb_btn_8,
    ieid_kb_btn_9,

    ieid_kb_btn_a,
    ieid_kb_btn_b,
    ieid_kb_btn_c,
    ieid_kb_btn_d,
    ieid_kb_btn_e,
    ieid_kb_btn_f,
    ieid_kb_btn_g,
    ieid_kb_btn_h,
    ieid_kb_btn_i,
    ieid_kb_btn_j,
    ieid_kb_btn_k,
    ieid_kb_btn_l,
    ieid_kb_btn_m,
    ieid_kb_btn_n,
    ieid_kb_btn_o,
    ieid_kb_btn_p,
    ieid_kb_btn_q,
    ieid_kb_btn_r,
    ieid_kb_btn_s,
    ieid_kb_btn_t,
    ieid_kb_btn_u,
    ieid_kb_btn_v,
    ieid_kb_btn_w,
    ieid_kb_btn_x,
    ieid_kb_btn_y,
    ieid_kb_btn_z,

    ieid_kb_btn_f1,
    ieid_kb_btn_f2,
    ieid_kb_btn_f3,
    ieid_kb_btn_f4,
    ieid_kb_btn_f5,
    ieid_kb_btn_f6,
    ieid_kb_btn_f7,
    ieid_kb_btn_f8,
    ieid_kb_btn_f9,
    ieid_kb_btn_f10,
    ieid_kb_btn_f11,
    ieid_kb_btn_f12,

    ieid_kb_btn_printscreen,
    ieid_kb_btn_scrolllock,
    ieid_kb_btn_pause,
    ieid_kb_btn_insert,
    ieid_kb_btn_home,
    ieid_kb_btn_pageup,

    ieid_kb_btn_end,
    ieid_kb_btn_pagedown,
    ieid_kb_btn_right,
    ieid_kb_btn_left,
    ieid_kb_btn_down,
    ieid_kb_btn_up,

    ieid_kb_btn_delete
};

struct input_event
{
    input_event_type type;
    input_event_id id;
    float amp = 0.f;
};

bool
operator==(const input_event& l, const input_event& r) noexcept;

struct input_action_descriptior;

struct input_scaled_action_handler : utils::generic_event_handler<void, float>
{
    input_action_descriptior* ref = nullptr;
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
    void
    register_scaled_action(const utils::id& id, real_obj* o, real_method m)
    {
        input_scaled_action_handler ev;
        ev.assign(o, m);

        auto& e = m_scaled_value_actions;
        auto& action = m_input_actions[id];

        for (auto& kb_action : action.m_triggers)
        {
            ev.ref = &kb_action.second;
            e[kb_action.first].push_back(ev);
        }
    }

    template <typename real_obj, typename real_method>
    void
    register_fixed_action(const utils::id& id, bool pressed, real_obj* o, real_method m)
    {
        input_fixed_action_handler ev;
        ev.assign(o, m);

        auto& e = m_fixed_actions_mapping[pressed ? input_event_type::input_event_press
                                                  : input_event_type::input_event_release];
        auto& action = m_input_actions[id];

        for (auto& kb_action : action.m_triggers)
        {
            e[kb_action.first].push_back(ev);
        }
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
        return m_input_event_state[id];
    }

protected:
    void
    drop_fired_event();

    void
    drop_obsolete();

    void
    consume_sdl_events(const SDL_Event& e);

    bool
    transform_from_sdl_event(const SDL_Event& se, std::vector<input_event>& v);

    mouse_state m_mouse_axis_state;
    mouse_wheel_state m_mouse_wheel_state;

    float m_dur_seconds = 0.f;

    std::unordered_map<input_event_id, bool> m_input_event_state;

    std::unordered_map<utils::id, input_action> m_input_actions;

    std::vector<std::unordered_map<input_event_id, std::vector<input_fixed_action_handler>>>
        m_fixed_actions_mapping;

    std::unordered_map<input_event_id, std::vector<input_scaled_action_handler>>
        m_scaled_value_actions;

    std::vector<input_event> m_events_to_fire;
};

}  // namespace engine

namespace glob
{
struct input_manager : public singleton_instance<::agea::engine::input_manager, input_manager>
{
};
}  // namespace glob
}  // namespace agea
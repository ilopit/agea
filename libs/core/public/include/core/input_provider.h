#pragma once

#include <utils/id.h>
#include <utils/generic_event_handler.h>

namespace kryga
{
namespace core
{

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

struct input_scaled_handler_data : utils::generic_event_handler<void, float>
{
    float basic_amp = 1.f;
};

struct input_fixed_handler_data : utils::generic_event_handler<void>
{
};

class input_provider
{
public:
    virtual ~input_provider() = default;

    template <typename real_obj, typename real_method>
    bool
    register_scaled_action(const utils::id& id, real_obj* o, real_method m)
    {
        input_scaled_handler_data h;
        h.assign(o, m);
        return do_register_scaled(id, h);
    }

    template <typename real_obj, typename real_method>
    bool
    register_fixed_action(const utils::id& id, bool pressed, real_obj* o, real_method m)
    {
        input_fixed_handler_data h;
        h.assign(o, m);
        return do_register_fixed(id, pressed, h);
    }

    virtual bool
    get_input_state(input_event_id id) = 0;

    virtual void
    unregister_owner(void* owner) = 0;

protected:
    virtual bool
    do_register_scaled(const utils::id& id, input_scaled_handler_data handler) = 0;

    virtual bool
    do_register_fixed(const utils::id& id, bool pressed, input_fixed_handler_data handler) = 0;
};

}  // namespace core

namespace glob
{
core::input_provider*
get_input_provider();

void
set_input_provider(core::input_provider* p);
}  // namespace glob

}  // namespace kryga

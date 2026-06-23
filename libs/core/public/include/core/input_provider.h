#pragma once

#include <utils/id.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

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

class input_provider;

// Live, global input state. ONE instance, owned by the provider and handed to handlers
// BY REFERENCE — never copied per event. It can grow (full held-key table, modifiers,
// gamepad axes…) without bloating the event queue. Use it for "what is true right now"
// queries, including multi-key actions: is_down(keyboard_w) && is_down(keyboard_lshift).
struct io_state
{
    input_provider* provider = nullptr;  // backs is_down()
    int32_t mouse_x = 0;                  // current cursor (window px, top-left)
    int32_t mouse_y = 0;

    bool
    is_down(input_event_id id) const;  // defined out-of-line — needs input_provider
};

// Global per-frame input delta — what changed THIS frame. Accumulated as events arrive
// and reset each tick. ONE instance, referenced by the context (never copied per event).
struct io_delta
{
    int32_t mouse_dx = 0;  // accumulated cursor motion this frame (raw px)
    int32_t mouse_dy = 0;
    int32_t wheel = 0;     // accumulated wheel ticks this frame

    // Ids whose down-state flipped THIS frame. Pure delta — it does NOT say which way:
    // pair it with the current state to recover direction (current.is_down(id) == true
    // means it just went down, false means it just went up). Enough for chords/combos.
    std::vector<input_event_id> changed;

    bool
    did_change(input_event_id id) const
    {
        return std::find(changed.begin(), changed.end(), id) != changed.end();
    }

    void
    reset()
    {
        mouse_dx = mouse_dy = wheel = 0;
        changed.clear();
    }
};

// Handed to handlers that opt in: the two live global states BY REFERENCE — current
// absolute state and this frame's delta. Nothing is copied per event; the per-event
// information (which trigger, and press / release / scaled) is implied by the handler
// that fires.
struct io_context
{
    const io_state& current;
    const io_delta& delta;
};

class input_provider
{
public:
    virtual ~input_provider() = default;

    // Stored, type-erased handlers. The dispatcher always has an io_context; the adapter
    // below drops it for handlers that don't take one.
    using fixed_handler = std::function<void(const io_context&)>;
    using scaled_handler = std::function<void(float, const io_context&)>;

    // Opt-in context — the bound method may take the io_context or omit it:
    //   fixed : void()      | void(const io_context&)
    //   scaled: void(float) | void(float, const io_context&)
    template <typename real_obj, typename real_method>
    bool
    register_scaled_action(const utils::id& id, real_obj* o, real_method m)
    {
        return do_register_scaled(id, wrap_scaled(o, m), reinterpret_cast<void*>(o));
    }

    template <typename real_obj, typename real_method>
    bool
    register_fixed_action(const utils::id& id, bool pressed, real_obj* o, real_method m)
    {
        return do_register_fixed(id, pressed, wrap_fixed(o, m), reinterpret_cast<void*>(o));
    }

    virtual bool
    get_input_state(input_event_id id) = 0;

    virtual void
    unregister_owner(void* owner) = 0;

protected:
    virtual bool
    do_register_scaled(const utils::id& id, scaled_handler handler, void* owner) = 0;

    virtual bool
    do_register_fixed(const utils::id& id, bool pressed, fixed_handler handler, void* owner) = 0;

private:
    template <typename real_obj, typename real_method>
    static fixed_handler
    wrap_fixed(real_obj* o, real_method m)
    {
        if constexpr (std::is_invocable_v<real_method, real_obj*, const io_context&>)
        {
            return [o, m](const io_context& c) { (o->*m)(c); };
        }
        else
        {
            return [o, m](const io_context&) { (o->*m)(); };
        }
    }

    template <typename real_obj, typename real_method>
    static scaled_handler
    wrap_scaled(real_obj* o, real_method m)
    {
        if constexpr (std::is_invocable_v<real_method, real_obj*, float, const io_context&>)
        {
            return [o, m](float v, const io_context& c) { (o->*m)(v, c); };
        }
        else
        {
            return [o, m](float v, const io_context&) { (o->*m)(v); };
        }
    }
};

inline bool
io_state::is_down(input_event_id id) const
{
    return provider && provider->get_input_state(id);
}

}  // namespace core

namespace glob
{
core::input_provider*
get_input_provider();

void
set_input_provider(core::input_provider* p);
}  // namespace glob

}  // namespace kryga

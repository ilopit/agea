#pragma once

#include <core/model_fwds.h>
#include <core/caches/line_cache.h>
#include <core/input_provider.h>

#include <utils/id.h>
#include <utils/path.h>
#include <utils/check.h>

#include <vfs/rid.h>

#include <SDL_events.h>

#include <unordered_map>
#include <vector>

union SDL_Event;

namespace kryga
{
namespace engine
{

using core::input_event_id;

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

// Translates SDL events into an ORDERED queue of input_records, drained once per frame
// in arrival order so handler invocation preserves the order events actually happened
// (two clicks, click-then-key, …) and so several same-frame occurrences are no longer
// collapsed into one. Continuous "held" state (is-the-button-down for camera orbit) is
// tracked separately in m_is_down and answered by get_input_state — that is level
// state, not an edge, and must not ride the event queue.
class input_manager : public core::input_provider
{
public:
    input_manager();

    bool
    input_tick(float dt);

    bool
    load_actions(const vfs::rid& rid);

    void
    fire_input_event();

    bool
    get_input_state(input_event_id id) override
    {
        auto it = m_is_down.find(id);
        return it != m_is_down.end() && it->second;
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
    do_register_scaled(const utils::id& id, scaled_handler handler, void* owner) override;

    bool
    do_register_fixed(const utils::id& id, bool pressed, fixed_handler handler, void* owner) override;

    void
    consume_sdl_events(const SDL_Event& e);

private:
    // A bound handler plus the owner pointer unregister_owner() matches on.
    struct fixed_entry
    {
        void* owner = nullptr;
        fixed_handler fn;
    };
    struct scaled_entry
    {
        void* owner = nullptr;
        scaled_handler fn;
        float basic_amp = 1.f;  // per-trigger amplitude from the action config
    };

    // Handlers bound to one trigger event id.
    struct event_handlers
    {
        std::vector<fixed_entry> pressed;
        std::vector<fixed_entry> released;
        std::vector<scaled_entry> scaled;
    };

    // One queued occurrence. Carries only its identity — which trigger and which edge.
    // No value payload: handlers read the live global current/delta at dispatch. The
    // queue exists purely to preserve ORDER of edges across the frame.
    struct input_record
    {
        enum class kind
        {
            pressed,
            released,
            scaled
        };

        input_event_id id = input_event_id::nan;
        kind k = kind::pressed;
        float amount = 0.f;  // scaled: raw scale (1.0 held key, normalized delta motion/wheel)
    };

    std::unordered_map<input_event_id, event_handlers> m_handlers;
    std::unordered_map<utils::id, input_action> m_input_actions;

    // FIFO of this frame's events, drained (and cleared) in fire_input_event.
    std::vector<input_record> m_queue;

    // Continuous press state, for get_input_state and per-frame scaled synthesis.
    std::unordered_map<input_event_id, bool> m_is_down;

    mouse_state m_mouse_axis_state;
    mouse_wheel_state m_mouse_wheel_state;

    // The two live global states handed to handlers by reference. m_current.provider is
    // wired to this manager in the ctor; both are kept current as events arrive and
    // m_delta is reset each tick.
    core::io_state m_current;
    core::io_delta m_delta;

    float m_dur_seconds = 0.f;
};

}  // namespace engine
}  // namespace kryga

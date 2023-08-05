#include "engine/input_manager.h"

#include <serialization/serialization.h>
#include <native/native_window.h>
#include <utils/agea_log.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <imgui.h>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

namespace agea
{
glob::input_manager::type glob::input_manager::type::s_instance;

namespace engine
{

namespace
{

const std::unordered_map<std::string, input_event_id> mapping{
    {"mouse_move_x", input_event_id::mouse_move_x},
    {"mouse_move_y", input_event_id::mouse_move_y},

    {"mouse_button_left", input_event_id::mouse_left},
    {"mouse_button_middle", input_event_id::mouse_wheel},
    {"mouse_button_right", input_event_id::mouse_right},

    {"return", input_event_id::keyboard_return},
    {"escape", input_event_id::keyboard_escape},
    {"backspace", input_event_id::keyboard_backspace},
    {"tab", input_event_id::keyboard_tab},
    {"space", input_event_id::keyboard_space},
    {"capslock", input_event_id::keyboard_capslock},

    {"0", input_event_id::keyboard_0},
    {"1", input_event_id::keyboard_1},
    {"2", input_event_id::keyboard_2},
    {"3", input_event_id::keyboard_3},
    {"4", input_event_id::keyboard_4},
    {"5", input_event_id::keyboard_5},
    {"6", input_event_id::keyboard_6},
    {"7", input_event_id::keyboard_7},
    {"8", input_event_id::keyboard_8},
    {"9", input_event_id::keyboard_9},

    {"a", input_event_id::keyboard_a},
    {"b", input_event_id::keyboard_b},
    {"c", input_event_id::keyboard_c},
    {"d", input_event_id::keyboard_d},
    {"e", input_event_id::keyboard_e},
    {"f", input_event_id::keyboard_f},
    {"g", input_event_id::keyboard_g},
    {"h", input_event_id::keyboard_h},
    {"i", input_event_id::keyboard_i},
    {"g", input_event_id::keyboard_j},
    {"k", input_event_id::keyboard_k},
    {"l", input_event_id::keyboard_l},
    {"m", input_event_id::keyboard_m},
    {"n", input_event_id::keyboard_n},
    {"o", input_event_id::keyboard_o},
    {"p", input_event_id::keyboard_p},
    {"q", input_event_id::keyboard_q},
    {"r", input_event_id::keyboard_r},
    {"s", input_event_id::keyboard_s},
    {"t", input_event_id::keyboard_t},
    {"u", input_event_id::keyboard_u},
    {"v", input_event_id::keyboard_v},
    {"w", input_event_id::keyboard_w},
    {"x", input_event_id::keyboard_x},
    {"y", input_event_id::keyboard_y},
    {"z", input_event_id::keyboard_z},

    {"f1", input_event_id::keyboard_f1},
    {"f2", input_event_id::keyboard_f2},
    {"f3", input_event_id::keyboard_f3},
    {"f4", input_event_id::keyboard_f4},
    {"f5", input_event_id::keyboard_f5},
    {"f6", input_event_id::keyboard_f6},
    {"f7", input_event_id::keyboard_f7},
    {"f8", input_event_id::keyboard_f8},
    {"f9", input_event_id::keyboard_f9},
    {"f10", input_event_id::keyboard_f10},
    {"f11", input_event_id::keyboard_f11},
    {"f12", input_event_id::keyboard_f12},

    {"printscreen", input_event_id::keyboard_printscreen},
    {"scrolllock", input_event_id::keyboard_scrolllock},
    {"pause", input_event_id::keyboard_pause},
    {"insert", input_event_id::keyboard_insert},
    {"home", input_event_id::keyboard_home},
    {"pageup", input_event_id::keyboard_pageup},

    {"end", input_event_id::keyboard_end},
    {"pagedown", input_event_id::keyboard_pagedown},
    {"right", input_event_id::keyboard_right},
    {"left", input_event_id::keyboard_left},
    {"down", input_event_id::keyboard_down},
    {"up", input_event_id::keyboard_up},

    {"delete", input_event_id::keyboard_delete}

};

bool
from_sdl_kb_sym_code(SDL_Keycode key_code, input_event_id& eie)
{
    switch (key_code)
    {
    case SDLK_0:
    case SDLK_1:
    case SDLK_2:
    case SDLK_3:
    case SDLK_4:
    case SDLK_5:
    case SDLK_6:
    case SDLK_7:
    case SDLK_8:
    case SDLK_9:
        eie = (input_event_id)(key_code - SDLK_0 + input_event_id::keyboard_0);
        return true;

    case SDLK_a:
    case SDLK_b:
    case SDLK_c:
    case SDLK_d:
    case SDLK_e:
    case SDLK_f:
    case SDLK_g:
    case SDLK_h:
    case SDLK_i:
    case SDLK_j:
    case SDLK_k:
    case SDLK_l:
    case SDLK_m:
    case SDLK_n:
    case SDLK_o:
    case SDLK_p:
    case SDLK_q:
    case SDLK_r:
    case SDLK_s:
    case SDLK_t:
    case SDLK_u:
    case SDLK_v:
    case SDLK_w:
    case SDLK_x:
    case SDLK_y:
    case SDLK_z:
        eie = (input_event_id)(key_code - SDLK_a + input_event_id::keyboard_a);
        return true;

    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
    case SDLK_F9:
    case SDLK_F10:
    case SDLK_F11:
    case SDLK_F12:
        eie = (input_event_id)(key_code - SDLK_F1 + input_event_id::keyboard_f1);
        return true;

    case SDLK_PRINTSCREEN:
    case SDLK_SCROLLLOCK:
    case SDLK_PAUSE:
    case SDLK_INSERT:
    case SDLK_HOME:
    case SDLK_PAGEUP:
        eie = (input_event_id)(key_code - SDLK_PRINTSCREEN + input_event_id::keyboard_printscreen);
        return true;

    case SDLK_END:
    case SDLK_PAGEDOWN:
    case SDLK_RIGHT:
    case SDLK_LEFT:
    case SDLK_DOWN:
    case SDLK_UP:
        eie = (input_event_id)(key_code - SDLK_END + input_event_id::keyboard_end);
        return true;
    }

    return false;
}

bool
from_sdl_mouse_btm_code(Uint8 mouse_button, input_event_id& eie)
{
    if (mouse_button > 0 && mouse_button <= 3)
    {
        eie = (input_event_id)mouse_button;
        return true;
    }
    return false;
}

input_event_id
from_string(const std::string& s)
{
    auto itr = mapping.find(s);

    return itr != mapping.end() ? itr->second : input_event_id::nan;
}

}  // namespace

input_manager::input_manager()
{
}

bool
input_manager::input_tick(float dur_seconds)
{
    m_dur_seconds = dur_seconds;
    m_to_drop_events.clear();

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT)
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard)
        {
            consume_sdl_events(e);
        }
    }

    return true;
}

bool
input_manager::load_actions(const utils::path& path)
{
    serialization::conteiner c;

    if (!serialization::read_container(path, c))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    auto actions = c["actions"];
    auto actions_size = actions.size();

    for (unsigned si = 0; si < actions_size; ++si)
    {
        auto a = actions[si];

        auto id = AID(a["id"].as<std::string>());

        auto events = a["events"];
        auto events_size = events.size();

        auto& action = m_input_actions[id];

        AGEA_check(!action.id.valid(), "Should not duplicate");

        action.id = id;

        for (unsigned ei = 0; ei < events_size; ++ei)
        {
            auto e = events[ei];

            auto trigger = e["trigger"].as<std::string>();
            auto native_code = from_string(trigger);

            if (native_code == input_event_id::nan)
            {
                ALOG_WARN("unknow trigger {0}", trigger);
                continue;
            }

            AGEA_check(native_code, "Should be valid");

            action.m_triggers[native_code].id = native_code;

            auto amp = e["amp"];

            if (amp.IsScalar())
            {
                action.m_triggers[native_code].amp = amp.as<float>();
            }
        }
    }
    return true;
}

void
input_manager::fire_input_event()
{
    for (auto a : m_active_events)
    {
        if (a->fire_on_start)
        {
            for (auto& h : a->m_registered_pres_fixed_handlers)
            {
                h.fire();
            }

            a->fire_on_start = false;
        }

        if (a->to_drop)
        {
            for (auto& h : a->m_registered_pres_fixed_handlers)
            {
                h.fire();
            }
        }

        for (auto& h : a->m_registered_scaled_handlers)
        {
            h.fire(a->extran_ampl * m_dur_seconds * h.basic_amp);
        }
    }

    drop_fired_event();
}

void
input_manager::drop_fired_event()
{
    for (auto e : m_to_drop_events)
    {
        m_active_events.erase(e);
        e->reset();
    }
}

void
input_manager::consume_sdl_events(const SDL_Event& sdle)
{
    input_event_id id;

    switch (sdle.type)
    {
    case SDL_KEYDOWN:
    {
        if (!from_sdl_kb_sym_code(sdle.key.keysym.sym, id))
        {
            return;
        }

        auto* es = &m_events_state[id];
        es->is_active = true;
        es->to_drop = false;

        m_active_events.insert(es);

        break;
    }
    case SDL_KEYUP:
    {
        if (!from_sdl_kb_sym_code(sdle.key.keysym.sym, id))
        {
            return;
        }

        auto* es = &m_events_state[id];
        es->to_drop = true;
        m_to_drop_events.push_back(es);

        break;
    }
    case SDL_MOUSEWHEEL:
    {
        m_mouse_wheel_state.x = sdle.wheel.x;
        m_mouse_wheel_state.y = sdle.wheel.y;

        id = input_event_id::mouse_move_wheel;

        if (sdle.wheel.y)
        {
            auto* es = &m_events_state[id];

            es->extran_ampl = (float)sdle.wheel.y;

            es->is_active = true;
            es->to_drop = false;

            m_active_events.insert(es);
            m_to_drop_events.push_back(es);
        }
        break;
    }
    case SDL_MOUSEMOTION:

        m_mouse_axis_state.x = sdle.motion.x;
        m_mouse_axis_state.y = sdle.motion.y;
        m_mouse_axis_state.xrel = sdle.motion.xrel;
        m_mouse_axis_state.yrel = sdle.motion.yrel;

        id = input_event_id::mouse_move_x;

        if (sdle.motion.xrel)
        {
            auto* es = &m_events_state[id];

            auto rel = (500.f * sdle.motion.xrel) / (float)glob::native_window::get()->get_size().w;
            es->extran_ampl = rel * glob::native_window::get()->aspect_ratio();

            es->is_active = true;
            es->to_drop = false;

            m_active_events.insert(es);
            m_to_drop_events.push_back(es);
        }

        id = input_event_id::mouse_move_y;
        if (sdle.motion.yrel)
        {
            auto* es = &m_events_state[id];

            auto rel = (500.f * sdle.motion.yrel) / (float)glob::native_window::get()->get_size().h;
            es->extran_ampl = rel;

            es->is_active = true;
            es->to_drop = false;

            m_active_events.insert(es);
            m_to_drop_events.push_back(es);
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
    {
        if (!from_sdl_mouse_btm_code(sdle.button.button, id))
        {
            return;
        }

        auto* es = &m_events_state[id];
        es->is_active = true;
        es->to_drop = false;

        m_active_events.insert(es);

        break;
    }
    case SDL_MOUSEBUTTONUP:
    {
        if (!from_sdl_mouse_btm_code(sdle.button.button, id))
        {
            return;
        }

        auto* es = &m_events_state[id];
        es->to_drop = true;
        m_to_drop_events.push_back(es);

        break;
    }

    default:
        return;
    }
}

}  // namespace engine
}  // namespace agea
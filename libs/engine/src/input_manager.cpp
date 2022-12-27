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
    {"mouse_move_x", input_event_id::ieid_ms_move_x},
    {"mouse_move_y", input_event_id::ieid_ms_move_y},

    {"mouse_button_left", input_event_id::ieid_ms_btm_left},
    {"mouse_button_middle", input_event_id::ieid_ms_btm_wheel},
    {"mouse_button_right", input_event_id::ieid_ms_btm_right},

    {"return", input_event_id::ieid_kb_btn_return},
    {"escape", input_event_id::ieid_kb_btn_escape},
    {"backspace", input_event_id::ieid_kb_btn_backspace},
    {"tab", input_event_id::ieid_kb_btn_tab},
    {"space", input_event_id::ieid_kb_btn_space},
    {"capslock", input_event_id::ieid_kb_btn_capslock},

    {"0", input_event_id::ieid_kb_btn_0},
    {"1", input_event_id::ieid_kb_btn_1},
    {"2", input_event_id::ieid_kb_btn_2},
    {"3", input_event_id::ieid_kb_btn_3},
    {"4", input_event_id::ieid_kb_btn_4},
    {"5", input_event_id::ieid_kb_btn_5},
    {"6", input_event_id::ieid_kb_btn_6},
    {"7", input_event_id::ieid_kb_btn_7},
    {"8", input_event_id::ieid_kb_btn_8},
    {"9", input_event_id::ieid_kb_btn_9},

    {"a", input_event_id::ieid_kb_btn_a},
    {"b", input_event_id::ieid_kb_btn_b},
    {"c", input_event_id::ieid_kb_btn_c},
    {"d", input_event_id::ieid_kb_btn_d},
    {"e", input_event_id::ieid_kb_btn_e},
    {"f", input_event_id::ieid_kb_btn_f},
    {"g", input_event_id::ieid_kb_btn_g},
    {"h", input_event_id::ieid_kb_btn_h},
    {"i", input_event_id::ieid_kb_btn_i},
    {"g", input_event_id::ieid_kb_btn_j},
    {"k", input_event_id::ieid_kb_btn_k},
    {"l", input_event_id::ieid_kb_btn_l},
    {"m", input_event_id::ieid_kb_btn_m},
    {"n", input_event_id::ieid_kb_btn_n},
    {"o", input_event_id::ieid_kb_btn_o},
    {"p", input_event_id::ieid_kb_btn_p},
    {"q", input_event_id::ieid_kb_btn_q},
    {"r", input_event_id::ieid_kb_btn_r},
    {"s", input_event_id::ieid_kb_btn_s},
    {"t", input_event_id::ieid_kb_btn_t},
    {"u", input_event_id::ieid_kb_btn_u},
    {"v", input_event_id::ieid_kb_btn_v},
    {"w", input_event_id::ieid_kb_btn_w},
    {"x", input_event_id::ieid_kb_btn_x},
    {"y", input_event_id::ieid_kb_btn_y},
    {"z", input_event_id::ieid_kb_btn_z},

    {"f1", input_event_id::ieid_kb_btn_f1},
    {"f2", input_event_id::ieid_kb_btn_f2},
    {"f3", input_event_id::ieid_kb_btn_f3},
    {"f4", input_event_id::ieid_kb_btn_f4},
    {"f5", input_event_id::ieid_kb_btn_f5},
    {"f6", input_event_id::ieid_kb_btn_f6},
    {"f7", input_event_id::ieid_kb_btn_f7},
    {"f8", input_event_id::ieid_kb_btn_f8},
    {"f9", input_event_id::ieid_kb_btn_f9},
    {"f10", input_event_id::ieid_kb_btn_f10},
    {"f11", input_event_id::ieid_kb_btn_f11},
    {"f12", input_event_id::ieid_kb_btn_f12},

    {"printscreen", input_event_id::ieid_kb_btn_printscreen},
    {"scrolllock", input_event_id::ieid_kb_btn_scrolllock},
    {"pause", input_event_id::ieid_kb_btn_pause},
    {"insert", input_event_id::ieid_kb_btn_insert},
    {"home", input_event_id::ieid_kb_btn_home},
    {"pageup", input_event_id::ieid_kb_btn_pageup},

    {"end", input_event_id::ieid_kb_btn_end},
    {"pagedown", input_event_id::ieid_kb_btn_pagedown},
    {"right", input_event_id::ieid_kb_btn_right},
    {"left", input_event_id::ieid_kb_btn_left},
    {"down", input_event_id::ieid_kb_btn_down},
    {"up", input_event_id::ieid_kb_btn_up},

    {"delete", input_event_id::ieid_kb_btn_delete}

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
        eie = (input_event_id)(key_code - SDLK_0 + input_event_id::ieid_kb_btn_0);
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
        eie = (input_event_id)(key_code - SDLK_a + input_event_id::ieid_kb_btn_a);
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
        eie = (input_event_id)(key_code - SDLK_F1 + input_event_id::ieid_kb_btn_f1);
        return true;

    case SDLK_PRINTSCREEN:
    case SDLK_SCROLLLOCK:
    case SDLK_PAUSE:
    case SDLK_INSERT:
    case SDLK_HOME:
    case SDLK_PAGEUP:
        eie =
            (input_event_id)(key_code - SDLK_PRINTSCREEN + input_event_id::ieid_kb_btn_printscreen);
        return true;

    case SDLK_END:
    case SDLK_PAGEDOWN:
    case SDLK_RIGHT:
    case SDLK_LEFT:
    case SDLK_DOWN:
    case SDLK_UP:
        eie = (input_event_id)(key_code - SDLK_END + input_event_id::ieid_kb_btn_end);
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

    return itr != mapping.end() ? itr->second : ieid_nan;
}

}  // namespace

bool
operator==(const input_event& lhs, const input_event& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.type == rhs.type;
}

bool
input_manager::transform_from_sdl_event(const SDL_Event& se, std::vector<input_event>& v)
{
    input_event ie = {};

    switch (se.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        ie.type = se.type == SDL_KEYDOWN ? input_event_type::input_event_press
                                         : input_event_type::input_event_release;

        if (from_sdl_kb_sym_code(se.key.keysym.sym, ie.id))
        {
            return false;
        }

        ie.amp = m_dur_seconds;

        v.push_back(ie);
        m_input_event_state[ie.id] = se.type == SDL_KEYDOWN;
        break;
    case SDL_MOUSEWHEEL:

        m_mouse_wheel_state.x = se.wheel.x;
        m_mouse_wheel_state.y = se.wheel.y;

        ie.type = input_event_type::input_event_scale;
        ie.id = input_event_id::ieid_ms_wheel;

        if (se.wheel.y)
        {
            ie.amp = (float)se.wheel.y * m_dur_seconds;
            v.push_back(ie);
        }
        break;
    case SDL_MOUSEMOTION:

        m_mouse_axis_state.x = se.motion.x;
        m_mouse_axis_state.y = se.motion.y;
        m_mouse_axis_state.xrel = se.motion.xrel;
        m_mouse_axis_state.xrel = se.motion.yrel;

        ie.type = input_event_type::input_event_scale;

        ie.id = input_event_id::ieid_ms_move_x;
        if (se.motion.xrel)
        {
            auto rel =
                (500.f * se.motion.xrel) / (float)agea::glob::native_window::get()->get_size().w;
            ie.amp = rel * m_dur_seconds * agea::glob::native_window::get()->aspect_ratio();
            v.push_back(ie);
        }

        ie.id = input_event_id::ieid_ms_move_y;
        if (se.motion.yrel)
        {
            auto rel =
                (500.f * se.motion.yrel) / (float)agea::glob::native_window::get()->get_size().h;
            ie.amp = rel * m_dur_seconds * agea::glob::native_window::get()->aspect_ratio();
            v.push_back(ie);
        }

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        ie.type = se.type == SDL_MOUSEBUTTONDOWN ? input_event_type::input_event_press
                                                 : input_event_type::input_event_release;

        if (!from_sdl_mouse_btm_code(se.button.button, ie.id))
        {
            return false;
        }

        ie.amp = m_dur_seconds;
        m_input_event_state[ie.id] = se.type == SDL_MOUSEBUTTONDOWN;
        v.push_back(ie);
        break;
    default:
        return false;
    }
    return true;
}

input_manager::input_manager()
    : m_scaled_value_actions()
    , m_fixed_actions_mapping(input_event_type::input_event_count)
{
}

bool
input_manager::input_tick(float dur_seconds)
{
    m_dur_seconds = dur_seconds;

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

        for (unsigned ei = 0; ei < events_size; ++si)
        {
            auto e = events[ei];

            auto trigger = e["trigger"].as<std::string>();
            auto native_code = from_string(trigger);

            if (native_code == ieid_nan)
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
    drop_fired_event();

    for (auto& e : m_events_to_fire)
    {
        switch (e.type)
        {
        case input_event_type::input_event_scale:
        {
            auto subscribers = m_scaled_value_actions[e.id];
            for (auto& s : subscribers)
            {
                s.fire(e.amp * s.ref->amp);
            }
            break;
        }
        case input_event_type::input_event_press:
        {
            {
                auto& subscribers = m_fixed_actions_mapping[e.type][e.id];
                for (auto& s : subscribers)
                {
                    s.fire();
                }
            }
            {
                auto& subscribers = m_scaled_value_actions[e.id];
                for (auto& s : subscribers)
                {
                    s.fire(e.amp * s.ref->amp);
                }
            }
            break;
        }
        default:
        {
            auto& subscribers = m_fixed_actions_mapping[e.type][e.id];
            for (auto& s : subscribers)
            {
                s.fire();
            }
        }
        }
    }

    drop_obsolete();
}

void
input_manager::drop_fired_event()
{
    auto itr = std::remove_if(
        m_events_to_fire.begin(), m_events_to_fire.end(),
        [this](const input_event& o)
        { return o.type == input_event_type::input_event_press && !m_input_event_state[o.id]; });

    if (itr != m_events_to_fire.end())
    {
        m_events_to_fire.erase(itr);
    }
}

void
input_manager::drop_obsolete()
{
    auto itr = std::remove_if(m_events_to_fire.begin(), m_events_to_fire.end(),
                              [this](const input_event& o)
                              {
                                  return o.type == input_event_type::input_event_press ||
                                         o.type == input_event_type::input_event_scale;
                              });

    if (itr != m_events_to_fire.end())
    {
        m_events_to_fire.erase(itr);
    }
}

void
input_manager::consume_sdl_events(const SDL_Event& e)
{
    std::vector<input_event> ie_to_add;

    if (transform_from_sdl_event(e, ie_to_add))
    {
        for (auto& ie : ie_to_add)
        {
            bool add = true;
            for (auto& f : m_events_to_fire)
            {
                if (ie == f)
                {
                    f = ie;
                    add = false;
                    break;
                }
            }
            if (add)
            {
                m_events_to_fire.push_back(ie);
            }
        }
    }
}

}  // namespace engine
}  // namespace agea
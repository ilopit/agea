#pragma once

#include <core/model_fwds.h>

#include <utils/singleton_instance.h>

#include <vector>
#include <functional>
#include <string>
#include <array>

#include <imgui.h>

namespace agea
{

namespace ui
{

struct selection_context
{
    ImGuiTreeNodeFlags base_flags;
    int i = 0;
    int selected = -1;
};

class window
{
public:
    window(std::string name)
        : m_show(false)
        , m_str(std::move(name))
    {
    }

    virtual ~window(){};

    virtual void
    handle();

    bool
    handle_begin(int flags = 0);

    void
    handle_end();

    bool m_show;
    std::string m_str;
};

class performance_counters_window : public window
{
public:
    static const char*
    window_title()
    {
        return "Performance counters";
    }

    performance_counters_window()
        : window(window_title())
    {
    }

    void
    handle() override;

    double frame_avg = 0.0;
    double fps = 0.0;
    double input_avg = 0.0;
    double tick_avg = 0.0;
    double ui_tick_avg = 0.0;
    double consume_updates_avg = 0.0;
    double draw_avg = 0.0;
    int lock = 0;
};

class level_editor_window : public window
{
public:
    static const char*
    window_title()
    {
        return "Level editor";
    }

    level_editor_window()
        : window(window_title())
    {
    }

    void
    handle() override;

    void
    draw_oject(root::game_object* obj);
};

class components_editor : public window
{
public:
    static const char*
    window_title()
    {
        return "Components editor";
    }

    components_editor()
        : window(window_title())
    {
    }

    void
    show(root::game_object_component* obj)
    {
        m_obj = obj;
        m_show = true;
    }

    void
    handle() override;

    root::game_object_component* m_obj = nullptr;
};

class materials_selector : public window
{
public:
    static const char*
    window_title()
    {
        return "Materials selector";
    }

    materials_selector()
        : window(window_title())
    {
        m_filtering_text.fill('\0');
    }

    using selection_callback = std::function<void(const std::string&)>;

    void
    show(const selection_callback& cb)
    {
        m_selection_cb = cb;
        m_show = true;
    }

    void
    handle() override;

    std::array<char, 128> m_filtering_text;
    selection_callback m_selection_cb;
};

class ui
{
public:
    ui();

    ~ui();

    void
    init();

    void
    new_frame(float dt);

    std::unordered_map<std::string, std::unique_ptr<window>> m_winodws;
};

}  // namespace ui

namespace glob
{
struct ui : public ::agea::singleton_instance<::agea::ui::ui, ui>
{
};
}  // namespace glob

namespace ui
{
template <typename T>
static T*
get_window()
{
    return (T*)glob::ui::get()->m_winodws[T::window_title()].get();
}
}  // namespace ui
}  // namespace agea
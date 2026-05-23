#pragma once

#include <engine/action_queue.h>
#include <global_state/global_state.h>

#include <memory>
#include <unordered_map>
#include <string>

#include <imgui.h>

namespace kryga
{

namespace ui
{

class gizmo_editor;
class material_previewer;

class window
{
public:
    window(std::string name)
        : m_show(false)
        , m_str(std::move(name))
    {
    }

    virtual ~window() {};

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
    double culled_draws_avg = 0.0;
    double all_draws_avg = 0.0;
    double objects_avg = 0.0;
    int lock = 0;
};

class render_config_window : public window
{
public:
    static const char*
    window_title()
    {
        return "Render Config";
    }

    render_config_window()
        : window(window_title())
    {
    }

    void
    handle() override;
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

    material_previewer&
    get_material_previewer()
    {
        return *m_material_previewer;
    }

    std::unordered_map<std::string, std::unique_ptr<window>> m_windows;
    engine::action_queue m_actions;
    std::unique_ptr<gizmo_editor> m_gizmo_editor;
    std::unique_ptr<material_previewer> m_material_previewer;
    std::string m_imgui_ini_path;  // backing storage for io.IniFilename
};

}  // namespace ui
}  // namespace kryga

#pragma once

#include <engine/editor.h>
#include <engine/ui.h>
#include <engine/private/screenshot_capture.h>

#include <global_state/global_state.h>

namespace kryga::engine
{

class editor_system : public gs::system
{
public:
    std::string_view
    system_name() const override
    {
        return "editor";
    }
    std::span<const std::string_view>
    system_deps() const override
    {
        static constexpr std::string_view d[] = {"render", "model"};
        return d;
    }

    game_editor editor;
    ui::ui ui;
    screenshot_capture screenshot;
};

}  // namespace kryga::engine

namespace kryga::ui
{

template <typename T>
static T*
get_window()
{
    return (T*)glob::glob_state().getr_editor_system().ui.m_windows[T::window_title()].get();
}

}  // namespace kryga::ui

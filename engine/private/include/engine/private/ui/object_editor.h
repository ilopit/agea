#pragma once

#include "engine/ui.h"

namespace agea::ui
{

class object_editor : public window
{
public:
    static const char*
    window_title()
    {
        return "Object editor";
    }

    object_editor()
        : window(window_title())
    {
    }

    void
    show(root::smart_object* obj)
    {
        current_object = obj;
        m_show = true;
    }

    void
    draw_components(root::game_object_component* obj, selection_context& sc);

    void
    handle() override;

    root::smart_object* current_object = nullptr;
};

}  // namespace agea::ui
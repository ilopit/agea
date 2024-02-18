#pragma once

#include "engine/ui.h"

namespace agea::ui
{

class package_editor : public window
{
public:
    static const char*
    window_title()
    {
        return "Package editor";
    }

    package_editor()
        : window(window_title())
    {
    }

    void
    handle() override;

    void
    draw_package_obj(core::package* p, selection_context& sc);
};

}  // namespace agea::ui
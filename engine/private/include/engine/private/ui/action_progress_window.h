#pragma once

#include "engine/ui.h"

namespace kryga
{
namespace ui
{

class action_progress_window : public window
{
public:
    static const char*
    window_title()
    {
        return "Actions";
    }

    action_progress_window()
        : window(window_title())
    {
    }

    void
    handle() override;
};

}  // namespace ui
}  // namespace kryga

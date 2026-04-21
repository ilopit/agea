#pragma once

#include "engine/ui.h"

#include <vfx/emitter_system.h>
#include <vfx/presets.h>

namespace kryga
{
namespace ui
{

class vfx_editor : public window
{
public:
    static const char*
    window_title()
    {
        return "VFX Editor";
    }

    vfx_editor();

    void
    handle() override;

private:
    void
    draw_toolbar();

    void
    draw_emitter_list();

    void
    draw_params(vfx::emitter& e);

    void
    draw_preview();

    vfx::emitter_system m_system;
    int m_selected = -1;
    int m_new_preset = 0;
    float m_zoom = 120.0f;
    bool m_paused = false;
    bool m_side_view = false;
};

}  // namespace ui
}  // namespace kryga

#pragma once

#include <imgui.h>
#include <ImGuizmo.h>

namespace kryga::ui
{

enum class gizmo_mode
{
    translate,
    rotate,
    scale
};

class gizmo_editor
{
public:
    void
    draw();

    bool
    is_using() const;

private:
    gizmo_mode m_mode = gizmo_mode::translate;
    ImGuizmo::MODE m_space = ImGuizmo::WORLD;
    bool m_using = false;
    bool m_snap_enabled = false;
    float m_snap_translate = 1.0f;
    float m_snap_rotate = 15.0f;
    float m_snap_scale = 0.1f;
};

}  // namespace kryga::ui

#pragma once
// Compatibility shims for deprecated ImGui APIs used by ImGuiColorTextEdit
#include "imgui.h"

namespace ImGui {
    inline ImGuiKey GetKeyIndex(ImGuiKey key) { return key; }
    inline void PushAllowKeyboardFocus(bool tab_stop) { PushItemFlag(ImGuiItemFlags_NoTabStop, !tab_stop); }
    inline void PopAllowKeyboardFocus() { PopItemFlag(); }
}

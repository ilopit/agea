#pragma once
// Compatibility shims for deprecated ImGui APIs used by ImGuiColorTextEdit.
// Also pulls in <algorithm> since TextEditor.h uses std::sort without
// including it (MSVC stdlib leaks it transitively, libc++ does not).
#include <algorithm>
#include "imgui.h"

namespace ImGui {
    inline ImGuiKey GetKeyIndex(ImGuiKey key) { return key; }
    inline void PushAllowKeyboardFocus(bool tab_stop) { PushItemFlag(ImGuiItemFlags_NoTabStop, !tab_stop); }
    inline void PopAllowKeyboardFocus() { PopItemFlag(); }
}

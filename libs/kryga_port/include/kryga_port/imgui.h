#pragma once

// Centralized wrapper for <imgui.h>.
//
// ImGui is a build-mode-axis dependency: the editor build pulls it in for
// the in-engine UI; the shipped game build (KRG_GAME) compiles without it
// so imgui_unofficial.lib is never linked into the binary. Including this
// header instead of <imgui.h> directly keeps that gate in one place.
//
// Usage:
//   #include <kryga_port/imgui.h>
//
//   #if KRG_EDITOR
//   ImGuiIO& io = ImGui::GetIO();   // safe — types are declared
//   #endif
//
// Code that *uses* ImGui types/functions still needs to be guarded with
// #if KRG_EDITOR — this header only handles the include side.

#if KRG_EDITOR
#include <imgui.h>
#endif

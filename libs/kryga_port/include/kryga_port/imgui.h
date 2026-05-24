#pragma once

#include <kryga_port/build_tier.h>

// Centralized wrapper for <imgui.h>.
//
// ImGui is available in any build with KRG_HAS_IMGUI=1 (editor and
// non-shipping game builds). Shipping builds (future KRG_SHIPPING=1)
// will strip ImGui entirely. Including this header instead of <imgui.h>
// directly keeps that gate in one place.
//
// Usage:
//   #include <kryga_port/imgui.h>
//
//   #if KRG_HAS_IMGUI
//   ImGuiIO& io = ImGui::GetIO();   // safe — types are declared
//   #endif
//
// Code that *uses* ImGui types/functions still needs to be guarded with
// #if KRG_HAS_IMGUI — this header only handles the include side.

#if KRG_HAS_IMGUI
#include <imgui.h>
#endif

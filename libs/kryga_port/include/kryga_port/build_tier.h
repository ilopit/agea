#pragma once

// Build-tier system — single source of truth for feature availability.
//
// CMake sets exactly ONE of these on each target:
//   KRG_BUILD_DEV   — editor (kryga_editor): full tooling, RPC, gizmos
//   KRG_BUILD_TEST  — game with dev tools (kryga_game): console, profiler, no editor UI
//   KRG_BUILD_SHIP  — shipping game: everything stripped
//
// This header derives all feature flags from the tier. Source code guards
// on the derived flags, never on the tier directly.
//
// Tier ordering: DEV > TEST > SHIP (each tier is a superset of the ones below).

// --- Validate: at most one tier set ------------------------------------------

#if (defined(KRG_BUILD_DEV) + defined(KRG_BUILD_TEST) + defined(KRG_BUILD_SHIP)) > 1
#error "Multiple build tiers set — define exactly one of KRG_BUILD_DEV/TEST/SHIP"
#endif

// --- Default tier: libraries that don't carry a tier treat all features as off.
// The tier propagates from the final executable's engine/render link — low-level
// libs (utils, core, vfs, ...) compile without one and never check KRG_HAS_*.

#if !defined(KRG_BUILD_DEV) && !defined(KRG_BUILD_TEST) && !defined(KRG_BUILD_SHIP)
#define KRG_BUILD_SHIP 1
#endif

// --- Derived feature flags ----------------------------------------------------
// Guard code with these, not with the tier defines.

#define KRG_HAS_IMGUI (KRG_BUILD_DEV || KRG_BUILD_TEST)
#define KRG_HAS_EDITOR KRG_BUILD_DEV
#define KRG_HAS_RPC KRG_BUILD_DEV
#define KRG_HAS_PROFILER (KRG_BUILD_DEV || KRG_BUILD_TEST)
#define KRG_HAS_CONSOLE KRG_HAS_IMGUI

// --- Legacy compat ------------------------------------------------------------
// Existing code checks #if KRG_EDITOR. Keep it working during migration.

#if KRG_BUILD_DEV
#define KRG_EDITOR 1
#endif

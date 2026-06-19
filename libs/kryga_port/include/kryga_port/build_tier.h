#pragma once

// Build-tier system — single source of truth for feature availability.
//
// CMake stamps exactly ONE tier on each engine/render target via KRG_TIER
// (use the kryga_set_tier() helper, never raw -D defines):
//   KRG_TIER_DEV   — editor (kryga_editor): full tooling, RPC, gizmos
//   KRG_TIER_GAME  — game with dev tools (kryga_game): console, profiler, no editor UI
//   KRG_TIER_SHIP  — shipping game: everything stripped
//
// KRG_TIER is a single, ALWAYS-defined integer. Because the tiers are ordered
// (DEV > GAME > SHIP) and each is a superset of the ones below, every feature
// flag is just a threshold compare. This keeps the flags usable in `#if`,
// `if constexpr`, and `static_assert` alike — no undefined-identifier traps.
//
// Source code guards on the derived KRG_HAS_* flags (or kryga::build::has_*),
// never on KRG_TIER directly.

// --- Tier constants -----------------------------------------------------------
// Higher = more features. Ordering is load-bearing (features compare with >=).

#define KRG_TIER_SHIP 1
#define KRG_TIER_GAME 2
#define KRG_TIER_DEV  3

// --- Default tier: libraries that don't carry a tier treat all features as off.
// The tier propagates from the final executable's engine/render link — low-level
// libs (utils, core, vfs, ...) compile without one and never check KRG_HAS_*.

#ifndef KRG_TIER
#define KRG_TIER KRG_TIER_SHIP
#endif

// --- Validate: tier must be a known value ------------------------------------

#if KRG_TIER != KRG_TIER_SHIP && KRG_TIER != KRG_TIER_GAME && KRG_TIER != KRG_TIER_DEV
#error "KRG_TIER set to an unknown value — use kryga_set_tier(target DEV|GAME|SHIP)"
#endif

// --- Derived feature flags ----------------------------------------------------
// Guard code with these, not with KRG_TIER. Each is 0/1 and always defined.

#define KRG_HAS_IMGUI    (KRG_TIER >= KRG_TIER_GAME)
#define KRG_HAS_PROFILER (KRG_TIER >= KRG_TIER_GAME)
#define KRG_HAS_EDITOR   (KRG_TIER >= KRG_TIER_DEV)
#define KRG_HAS_RPC      (KRG_TIER >= KRG_TIER_DEV)
#define KRG_HAS_CONSOLE  KRG_HAS_IMGUI

// --- constexpr companions -----------------------------------------------------
// For runtime / `if constexpr` branching where the preprocessor isn't needed.
// (Include guards and conditional #include still use the KRG_HAS_* macros.)

#ifdef __cplusplus
namespace kryga::build
{
inline constexpr int tier = KRG_TIER;

inline constexpr bool has_imgui = KRG_HAS_IMGUI;
inline constexpr bool has_profiler = KRG_HAS_PROFILER;
inline constexpr bool has_editor = KRG_HAS_EDITOR;
inline constexpr bool has_rpc = KRG_HAS_RPC;
inline constexpr bool has_console = KRG_HAS_CONSOLE;
}  // namespace kryga::build
#endif

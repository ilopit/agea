# 04 — Engine Roadmap

Keyed to the engine readiness audit. Every item is classified **keep**, **build**, **defer**, or **cut**, with a priority.

## Scoring legend (carried from audit)

1 = missing · 2 = stubs · 3 = working prototype · 4 = production-ready for this game · 5 = polished.

## Keep and use as-is

| Area | Current | Notes |
|------|---------|-------|
| Vulkan 1.2 render core + BDA | 4 | Already production-ready for this game. |
| Baked lighting / lightmaps | 4 | Central to the 3D low-poly look. |
| Shadow mapping | 4 | Directional shadow only needed; DPSM not required for v1. |
| Clustered / forward+ lights | 4 | Overkill for a puzzle game but costs nothing to keep. |
| glTF + custom 3DO mesh import | 3 | Sufficient for puzzle assets. |
| ImGui editor / tooling UI | 4 | Use for puzzle editor (internal only — not shipped UI). |
| Scene / level / package system | 3 | Fits menu/game/settings state flow. |
| `.apkg` asset packaging + VFS | 3 | Android VFS backend added in port plan. |
| Reflection (`ar` + argen.py) | 4 | Keep. Already integrated. |
| Validation layers support | 3 | Useful in dev builds on Android. |

## Build — P0 (blocks shipping this game)

Ordered by dependency, not calendar.

### Audio subsystem

- Pick **miniaudio** (header-only, permissive, minimal integration cost). Reconsider only if 3D audio becomes a requirement.
- Required features: SFX pool (short one-shots), ambient loop with simple fade, volume categories (SFX, ambient, UI).
- Android: route through OpenSL ES or AAudio via miniaudio backends. Handle focus loss (ducking) — OS compliance.
- Integrate with VFS for asset loading.
- Exit criterion: tapping a voxel in a debug scene plays a chime on desktop and Android without glitching.

### Player-facing UI system

- Custom minimal widget set: button, label, image, layout (row/column), modal. No accordion/tree/grid needed for this game.
- Alternative: integrate RmlUi. Benefits: HTML/CSS-like authoring, third-party support. Drawbacks: another large dependency, another rendering path, overkill for ~10 screens.
- Lean custom. Time-box 2 weeks; if it exceeds, re-evaluate RmlUi.
- Required screens: splash, main menu, pack select, puzzle, pause, settings, solved/summary.
- ImGui stays for editor/dev only — not shipped to players.

### Save / profile system

- Local only. Profile file + per-puzzle completion state + settings.
- Format: serialized flat struct (existing reflection could drive it) or plain JSON via yaml-cpp. Lean JSON — easier to debug by hand.
- Android: write to internal app storage, not external.
- Cloud save deferred.

### Touch input + gestures — partial

- ~~Add a touch path in `input_manager.cpp`.~~ Done — SDL2 `SDL_FINGER*` events handled, single-finger tap/drag mapped to mouse semantics via SDL hints.
- Remaining: long-press, two-finger drag (orbit), pinch (zoom) gesture layer.
- Abstract so desktop mouse/keyboard still drives the same actions — keep dev workflow unchanged.

### Application / scene state management

- `App -> MenuScene | PackSelectScene | PuzzleScene | SettingsScene` transitions.
- Already partially in place (level manager). Promote to an explicit state machine with push/pop.
- Exit criterion: menu → puzzle → solved → back to menu without leaks or render glitches.

### Event / messaging

- Promote `generic_event_handler.h` to a usable event bus.
- Scope small: typed events, no coroutine/async, no cross-thread dispatch.
- Used for UI button clicks, puzzle-solved notifications, save triggers.

### Puzzle editor (tool, not shipped)

- Lives inside engine.exe as an ImGui window. No separate binary.
- Features: load/save puzzle, edit voxel truth state, view clues, run solver to verify uniqueness, export to pack.
- Solver is the real work. See below.

### Puzzle solver / validator

- Library code consumed by both the editor and runtime (for hint generation).
- Inputs: grid dimensions, per-row clue counts, optional per-voxel constraints.
- Outputs: solution, uniqueness flag, deduction trace (for hint generation and difficulty scoring).
- Critical correctness component. Write tests against hand-solved small puzzles.

### Android build pipeline — partial

- Execute the mobile port plan (Doc 05). Phase 0 done, Phase 1 ~90% done, Phase 2 ~30% done (see Doc 05 for details).
- APK builds work. AAB signing pipeline not yet set up.
- Remaining: lifecycle management, gesture input, Vulkan surface handling, AAB CI.

## Build — P1 (needed for a polished v1 or soft launch)

| Area | Current | Target | Notes |
|------|---------|--------|-------|
| Particle system | 1 | 2–3 | Only needed for "solved" flourish. Simple CPU-driven emitter acceptable. *Note: libs/vfx with CPU emitter + ImGui editor exists on a feature branch but not merged to main. Needs render pipeline integration.* |
| ASTC texture pipeline | 1 | 3 | Big impact on APK size. Bake step in asset pipeline. |
| Crash reporting | 1 | 3 | Sentry (self-hosted possible) or Firebase Crashlytics. Sentry lean for privacy / portability. |
| Localization infra | 1 | 3 | String table + fallback to key. Integrate into UI system at build, not bolt-on after. |
| Safe area / notch | 1 | 3 | Read insets on Android; layout guides in UI system. |
| Audio ducking on focus loss | 1 | 3 | OS compliance — required, small work. |
| Battery / thermal awareness | 1 | 3 | Frame cap 30/60, pause when backgrounded. Already in android_port.md Phase 4. |
| AAB pipeline (CI) | 1 | 3 | GitHub Actions + Android NDK + keystore in secret. |

## Defer (post-launch or on-demand)

| Area | Reason to defer |
|------|-----------------|
| Dynamic resolution scaling | Only needed if thermal throttling is observed on target hardware. Measure first. |
| Post-processing (bloom/tonemap/grading) | Low-poly + baked GI is already the visual identity. A light tonemap is the only likely addition. |
| Cloud save | Post-launch nice-to-have. Not a retention-critical feature for a single-player offline puzzle. |
| Hot-reload | Dev convenience. Nice, not blocking. |
| Analytics SDK | Post-launch. Soft launch without analytics is acceptable for a first release; D1/D7 math can come from Play Console basics. |
| IAP SDK | Required *if* premium-pack model ships in v1. If v1 ships as a single purchase or free+limited, deferred. |
| Ads SDK | Required only if freemium model wins (see Doc 03). Leaner to skip for v1. |
| 3D positional audio | Not needed for this game's design. |
| Order-independent transparency | No transparent assets planned beyond simple alpha UI. |

## Cut (remove from roadmap for this game)

| Area | Current | Reason to cut |
|------|---------|---------------|
| `packages/tbs` (hex grid / TBS) | working | Not this game. Keep code in repo temporarily if preserving optionality; exclude from main build target. Revisit for a future title. |
| Skeletal animation / skinning | 3 | Voxel sudoku has no skinned meshes. Keep library, but cut from *this game's* priority list. |
| DPSM (dual-paraboloid shadow map) | working | No point-light shadow use case in this game. |
| Advanced particle (GPU-driven) | 1 | Simple CPU particle is enough for solved flourish. |
| Multi-color voxel layers | — | Out of scope per Doc 03. Reconsider in Expert pack post-v1. |

Cutting does **not** mean deleting. It means de-prioritizing and excluding from the critical path.

## Known technical debt

Tracked separately under [`../issues/`](../issues/). Includes mesh import bugs, shadow system limitations, validation-layer state, editor UI persistence, and package-organization refactoring. Priority is to ship this game; those items are fixed as they become blocking.

## Roadmap tensions and tradeoffs

- **Custom UI system vs. RmlUi** — faster-to-MVP vs. better-long-term. Revisit at 2-week mark.
- **miniaudio vs. FMOD/Wwise** — free + simple vs. pro tools + licensing. Miniaudio fits v1; switching cost to a pro tool is not huge if ever warranted.
- **Handcrafted puzzle content vs. generator** — correctness risk vs. content velocity. See Doc 03.
- **Keep skeletal animation built-out** — expensive to regress, cheap to park. Park it.
- **Post-process skipped** — visuals may read as flat on some devices. If QA flags this, add a single tonemap pass. Don't pre-build it.

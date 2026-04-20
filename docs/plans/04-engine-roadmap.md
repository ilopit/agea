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

### Touch input + gestures

- Add a touch path in `input_manager.cpp`. SDL2 provides `SDL_FINGER*` events on Android if SDL2 route is chosen.
- Minimum gestures: tap, long-press, two-finger drag (orbit), pinch (zoom).
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

### Android build pipeline

- Execute the mobile port plan (Doc 05).
- AAB output, signed, installable on a real phone.

## Build — P1 (needed for a polished v1 or soft launch)

| Area | Current | Target | Notes |
|------|---------|--------|-------|
| Particle system | 1 | 2–3 | Only needed for "solved" flourish. Simple CPU-driven emitter acceptable. |
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

Engine issues that don't block shipping this game but are explicit items to be aware of. Priority is to ship; fix these as they become blocking.

### Mesh system

- **UV2 always present in `vertex_data`** (~18% overhead for non-lightmapped meshes). Current struct is 52 bytes; would be 44 without UV2. Fix: split UV2 into a separate vertex stream (binding 1). Dummy buffer bound for non-lightmapped draws. Pattern used by Unreal's `FStaticMeshVertexBuffers`.
- **OBJ importer resets vertex buffer per shape.** `mesh_importer.cpp:53` resizes inside the shape loop — multi-shape OBJs lose all but the last shape. Fix: accumulate across shapes with offset.
- **OBJ importer UB on missing normals / texcoords / colors.** `mesh_importer.cpp:72-83` accesses attrib arrays without bounds checks. Fix: check `idx.*_index >= 0` and defaults.

### Shadow system

- **Shadow center hardcoded to origin** (`kryga_render_shadows.cpp:258`). Largest cascade is ~242 units — anything >240m from origin has no directional shadow. Fix: texel-snap the shadow center to the camera.
- **No frustum culling for shadow passes.** All `m_draw_batches` iterated per pass (4 CSM + up to 16 local = up to 20× geometry). Fix: per-pass frustum cull.
- **DPSM distortion near equator.** Seams between hemispheres, non-uniform resolution, PCF not uniform in world space. Fix: cube shadow maps (more memory) or VSM with a tetrahedral warp.
- **Constant bias causes acne + peter-panning simultaneously.** Fix: slope-scaled depth bias via Vulkan pipeline state (`depthBiasConstantFactor` / `depthBiasSlopeFactor`) or in-shader `max(bias, slope * tan(angle))`.
- **PCF mode is global** — local lights can't opt out of expensive PCF. A 7×7 PCF × 8 local lights = 392 shadow taps per fragment.
- **Cascade radius over-allocated** — wastes shadow-map resolution. Calculation doesn't account for camera offset from shadow-center origin.
- **Per-frame `std::vector` + `std::sort` in `select_shadowed_lights`** (`kryga_render_shadows.cpp:558`). Fix: pre-allocated fixed-size array.
- **Point-light view always looks at +Z.** DPSM quality poor if visible geometry isn't +Z-aligned. Fix: orient view toward camera or dominant geometry.
- **No shadow-map atlas — 320 MB of depth memory** (20 × 2048² × 4 bytes) allocated even when most local passes are empty. Fix: atlas into 1–2 large textures.
- **Cascade blending doubles taps** in the 10% transition zone (64 shadow taps with Poisson32).
- **No NaN guard on `acos(outer_cut_off)`** for spot lights. Invalid input propagates NaN through the shadow matrix.

### Validation / debug

- **Validation layer on in all builds** (~5–15% CPU overhead). Gate off on Release after the mobile branch lands.
- **GPU-Assisted Validation not enabled** — known friction with BDA + descriptor indexing; 2–10× frame-time hit. Keep as opt-in via CLI flag.
- **Synchronization Validation deferred** — doubles CPU validation cost. Enable during render-graph / barrier work only.
- **Best Practices layer** high signal on mobile — enable during Android perf passes only.
- **Debug Printf** mutually exclusive with GPU-AV in older SDKs. Add behind a dedicated shader-include helper when needed.
- **Dedupe key** is `(messageIdNumber << 32) ^ fnv1a(pMessage[:192])`. Hides new instances of the same VUID with different object handles. Fix: call `debug_reset_dedupe()` at scene boundaries.
- **Object naming** done for core + loader paths (device, queues, per-frame buffers, render passes, shader effects, compute, bindless pool). Still unnamed: per-frame descriptor sets (noise), cached `VkDescriptorSetLayout`s, framebuffers, bake intermediate buffers, UI vertex / index buffers.
- **Break-on-error Windows-only.** No Linux/macOS equivalent. Android: `raise(SIGTRAP)` when the port goes live.

### Editor

- **ImGui `CollapsingHeader` state not persisted between sessions.** `imgui.ini` only saves window-level state. Affects bake presets, render config, editor panels. Options: serialize to config file, use `ImGui::GetStateStorage()` manual save/load, or accept session-only.

### Package organization

- **Split packages for assets and module types.** Current `packages/` conflates content data with typed class modules. Split into separate asset packages and module-type packages for cleaner load ordering and reuse.

## Roadmap tensions and tradeoffs

- **Custom UI system vs. RmlUi** — faster-to-MVP vs. better-long-term. Revisit at 2-week mark.
- **miniaudio vs. FMOD/Wwise** — free + simple vs. pro tools + licensing. Miniaudio fits v1; switching cost to a pro tool is not huge if ever warranted.
- **Handcrafted puzzle content vs. generator** — correctness risk vs. content velocity. See Doc 03.
- **Keep skeletal animation built-out** — expensive to regress, cheap to park. Park it.
- **Post-process skipped** — visuals may read as flat on some devices. If QA flags this, add a single tonemap pass. Don't pre-build it.

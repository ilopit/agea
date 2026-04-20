# 05 — Mobile Port

Full plan for bringing kryga and the first game onto Android. Policy, execution phases, file changes, current state, and exit criteria all in one place.

## Scope

### In scope for v1

- **Android.** First and only shipped platform for v1.
- Minimum spec driven by engine requirements:
  - Android 11+ (API 30+).
  - Vulkan 1.2.
  - Buffer Device Address support (Adreno 640+ / Mali-G77+).
  - 3+ GB RAM.
  - arm64-v8a only. No armeabi-v7a, no x86.

### Out of scope for v1 (deferred)

- **iOS.** Deferred. Adds MoltenVK, Xcode + App Store Connect workflow, separate signing, separate testing hardware. Revisit after Android ships and the loop is validated.
- **ChromeOS / Android tablets.** Landscape layout not a v1 priority. If it happens, it happens — do not block on tablet polish.
- **Cloud streaming platforms** (GeForce Now, xCloud): not applicable; single-player offline puzzle.

## Platform integration status (from audit)

All currently at 1 (absent):

- Vulkan surface recreation on suspend/resume.
- Orientation change handling.
- Multi-touch + gestures.
- Safe area / notch handling.
- Back button / navigation gestures.
- Audio ducking.
- Battery / thermal awareness.

Collectively: the engine is a desktop demo today. Getting it to a shippable Android state is the single largest block of work. Scheduling in Doc 06.

## Vulkan compatibility

| Vulkan feature | Desktop | Android support |
|----------------|---------|-----------------|
| Vulkan 1.2 | Required | 85%+ of devices (API 24+) |
| Buffer Device Address | Required | Adreno 640+, Mali-G77+ |
| Descriptor indexing | Required | Most Vulkan 1.2 devices |
| `shaderInt64` | Removed | Replaced with uvec2 via `GL_EXT_buffer_reference_uvec2` |
| `VK_EXT_graphics_pipeline_library` | Removed | Was unused |

### Device-floor options

1. **Target high-end only** — Snapdragon 855+ (Adreno 640+), Exynos 990+ (Mali-G77+). Simpler, narrower reach.
2. **Add SSBO fallback render path** — descriptor-binding fallback for devices without BDA / `shaderInt64`. Wider reach, more code paths to maintain.

**Decision for v1:** option 1 (high-end only). Reassess after soft-launch data. The audience for this game is developed-markets mid-to-upper tier, so narrower reach is largely a non-issue.

### First step: validate hardware

```bash
# Get Vulkan capability dump from target device
adb shell dumpsys gpu | grep -A 100 "Vulkan"
# Or use a vulkaninfo APK
```

Do this before writing any port code. If target devices fail the capability check, the device-floor decision needs revisiting.

## Key technical decisions

Each has a default and a decide-by deadline. Tracked long-term in Doc 07.

1. **Windowing: SDL2 vs. GameActivity + native window.**
   - Default: SDL2 (already used on desktop). Less code for lifecycle, less platform glue, at the cost of another layer to debug.
   - Alternative: Direct ANativeWindow via `NativeActivity` or GameActivity (Jetpack). Full control, smaller APK, but more code for input and lifecycle.
   - Decide by: M2 start.
2. **Asset delivery: APK assets vs. Play Asset Delivery on first run.**
   - Default: APK assets while total size < 100 MB.
   - Decide by: M5.
3. **Crash reporting backend: Sentry vs. Firebase Crashlytics.**
   - Default: Sentry (privacy, no Firebase SDK pull-in).
   - Decide by: M5 start.
4. **Analytics.** Defer (Doc 04). Play Console stats are acceptable for v1.
5. **Signing + CI.** Keystore stored in GitHub Actions secrets; CI produces signed AAB on tag push. No manual signing from dev machine in the critical path.

## Game-specific port requirements

These are the items this specific game requires that deserve explicit callouts beyond the generic port:

- **Multi-touch with gestures** (not just tap). Two-finger orbit and pinch zoom are in the control scheme (Doc 03). Multi-touch is P0, not optional.
- **Portrait-first, rotation allowed.** Most Picross-audience players hold portrait. Landscape must not crash. Orientation change handled, not locked.
- **Audio focus handling.** OS compliance + audience expectation (players have music / podcasts playing). Wire audio subsystem (Doc 04) to Android's `AudioManager.OnAudioFocusChange`.
- **Background / foreground lifecycle preserves puzzle state.** Puzzle progress must survive the app being backgrounded for an hour. Save on `onPause`, restore on `onResume`.
- **No GPU work while in background.** Strict — drains battery fast otherwise.
- **Small APK.** Target <60 MB initial download, <150 MB install. ASTC + shared meshes + limited audio asset budget.

## Execution phases

### Phase 0 — Feasibility & requirements

- Validate Vulkan capabilities on target devices (command above).
- Confirm BDA availability on intended reference device.
- Confirm memory budget with VMA on target device.
- Lock device-floor decision (default: option 1 above).

### Phase 1 — Build system (~1–2 weeks)

- **Android NDK toolchain**: `cmake/android.toolchain.cmake`. Target `arm64-v8a` only.
- **Gradle project**: `android/` with `build.gradle.kts`, JNI bridge, APK packaging.
- **Cross-compile thirdparty**:
  - SDL2 from source (not prebuilt Windows binary path).
  - Most header-only libs need no changes.
  - Boost: strip to header-only subset, or replace Beast if possible.
- **Asset packaging strategy** (pick one, default Option A for small APK):
  - **Option A:** Assets in APK `assets/` folder → VFS mount from `AAssetManager`.
  - **Option B:** Download assets on first launch → VFS mount from internal storage.

### Phase 2 — Platform layer (~2–3 weeks)

- **Android entry point**: `libs/native/private/src/android/native_activity.cpp`. JNI callbacks: `onCreate`, `onResume`, `onPause`, `onDestroy`. Surface lifecycle management — can recreate without destroying `VkDevice`.
- **Windowing route**: per decision above. SDL2 default; `SDL_main` handles Activity lifecycle, rotation, keyboard.
- **VFS Android backend**: `libs/vfs/private/src/android_asset_backend.cpp`. Read from APK via `AAssetManager`. Write to internal storage for cache / generated content.
- **Input mapping**:
  - Touch → gesture layer (tap, long-press, two-finger orbit, pinch zoom).
  - Desktop mouse + keyboard continues to work unchanged via the same abstraction.

### Phase 3 — Vulkan adaptation (~1–2 weeks)

- **Surface creation**: add `VK_KHR_android_surface` path in `vulkan_render_device.cpp`. Already abstracted via SDL if using SDL2.
- **Extension negotiation**: runtime capability check for BDA / `shaderInt64`. SSBO fallback path behind a feature flag (deferred to post-v1 unless needed).
- **Swapchain adjustments**:
  - `VK_PRESENT_MODE_FIFO_KHR` (vsync, battery-friendly).
  - Handle pre-rotation to avoid compositor overhead.
  - Dynamic resolution for thermal throttling (see Phase 4).
- **Memory budget**:
  - VMA budget tracking for mobile constraints.
  - Texture streaming priority adjustments.

### Phase 4 — Mobile optimizations (ongoing)

- **Render quality settings**: resolution scaling, reduced shadow resolution, simplified lighting (fewer probes, smaller lightmaps). Most of these are knobs, not rewrites.
- **Battery / thermal**:
  - Frame rate cap (30/60 user-selectable).
  - Pause rendering when app backgrounded.
  - Reduce work when thermal throttling detected.
- **APK size**:
  - ASTC texture compression for Android.
  - Asset bundles for optional content if total size grows.

## File changes summary

### New files

| Path | Purpose |
|------|---------|
| `android/` | Gradle project, Java Activity |
| `cmake/android.toolchain.cmake` | NDK cross-compile toolchain |
| `libs/native/private/src/android/` | Android window + lifecycle |
| `libs/vfs/private/src/android_asset_backend.cpp` | APK asset access via `AAssetManager` |
| `engine/private/src/app/android_main.cpp` | JNI entry point |

### Modified files

| Path | Change |
|------|--------|
| `CMakeLists.txt` | Platform detection, Android target |
| `thirdparty/CMakeLists.txt` | SDL2 from source, skip Windows binary path |
| `libs/render/render/private/src/vulkan_render_device.cpp` | Optional extensions, Android surface creation |
| `engine/private/src/input_manager.cpp` | Touch event handling, gesture layer |

## Completed preparatory work

- Removed unused `VK_EXT_graphics_pipeline_library` requirement.
- Replaced `shaderInt64` with `GL_EXT_buffer_reference_uvec2` for BDA addresses.
- All BDA push constants now use `bda_addr` (uvec2) type.

## Current architecture reference

### Platform abstraction

- SDL2 for windowing (`libs/native/`).
- VFS with mountable backends (`libs/vfs/`).

### Vulkan setup

- VkBootstrap for instance / device creation.
- VMA for memory allocation.
- Requires Vulkan 1.2+.

### Key dependencies (Android-ready)

- GLM, spdlog, CLI11, cgltf, yaml-cpp — header-only or standard C++.
- SDL2 — needs NDK build.
- Boost — may need stripping.

## Port-specific risks

| Risk | Mitigation |
|------|-----------|
| BDA not available on low-end devices | Narrow the floor (default), or fall back to SSBO descriptor binding (set 1) instead of push-constant BDA. |
| SDL2 Android builds fragile | Switch to GameActivity + native window directly. Revisit at Phase 2. |
| Large APK size from assets | Android App Bundles + Play Asset Delivery. |
| Android lifecycle edge cases (surface loss, audio focus, low-memory kill) eat weeks | Real-device testing from day one of Phase 2. No reliance on emulator. |

## Test devices

Minimum physical device coverage to call the port "shipped":

- One Pixel-class device (Snapdragon 7xx/8xx, Adreno) at min-spec.
- One Samsung Galaxy Sxx (Exynos + Mali or Snapdragon) — distinct GPU vendor.
- One lower-spec device *at* the device floor to validate min-spec claims.

Emulators are not sufficient. Vulkan BDA behavior and thermal / lifecycle behavior only reproduce on real hardware.

## iOS — what it would cost later

Not committing, just noting for eyes-open planning:

- MoltenVK layer (mostly works; some extension gaps — BDA is supported on recent MoltenVK).
- Xcode project, CocoaPods or SwiftPM, Apple Developer account + annual fee.
- Swift / Objective-C bridge for app lifecycle and AppDelegate.
- Separate input + audio integration path.
- App Store Connect review (stricter than Play).
- Estimated 6–10 weeks of work on top of a shipped Android version.

Decision deferred until Android v1 has audience signal.

## Exit criteria for "mobile port done" (v1)

- AAB signed and uploaded to Play Console internal testing track.
- First puzzle pack playable end-to-end on two real devices covering distinct GPU vendors.
- No crash across: cold start, suspend/resume, rotation, low-memory kill + relaunch, incoming phone call, audio focus loss, airplane-mode toggle.
- Crash-free session rate ≥ 99% across an internal testing cohort of at least 10 users over 7 days.

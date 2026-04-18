# Android Port Plan

Created: 2026-04-14

## Overview

Port the Kryga Vulkan engine to Android. The architecture is well-suited due to clean abstraction layers (SDL2, VFS, VkBootstrap).

---

## Phase 0: Feasibility & Requirements

### Critical Vulkan Compatibility

Coverage anchored against Android Baseline Profile (ABP) data — ABP 2025 covers ~80% of active Vulkan devices but requires only Vulkan 1.1 and **no descriptor indexing**.

| Vulkan Feature | Desktop | Android Support | Notes |
|----------------|---------|-----------------|-------|
| Vulkan 1.2 | Required | ~85% of Vulkan devices | Above ABP 2025 floor; excludes pre-Android-11 devices and some budget chips |
| ~~`VK_EXT_graphics_pipeline_library`~~ | ~~Required~~ | Removed (was unused) | |
| Buffer Device Address (uvec2 path) | Required | Broad on A11+ flagships, varies | uvec2 path drops the `shaderInt64` gate; remaining gate is `bufferDeviceAddress` feature itself |
| ~~`shaderInt64`~~ | ~~Required~~ | Removed — using uvec2 via `GL_EXT_buffer_reference_uvec2` | |
| `runtimeDescriptorArray` | Required | Mali Valhall (G77+), Adreno 6xx+ | Not in any ABP |
| `descriptorBindingPartiallyBound` | Required | Same as above | Not in any ABP |
| `shaderSampledImageArrayNonUniformIndexing` | Required | Same as above | TBDR perf cost — fine if index is uniform per draw |
| `descriptorBindingSampledImageUpdateAfterBind` | **Required (riskiest gate)** | Spotty on Mali Bifrost (G71/72/76); fine on Valhall, modern Adreno | This single feature is the biggest residual blocker after BDA fix |
| `descriptorBindingVariableDescriptorCount` | Required | Same gate as descriptor indexing | Used by bindless layout in `kryga_render_init.cpp:833` |
| `scalarBlockLayout` | Required | Standard on Vulkan 1.2 | Low risk |

### Options

1. **High-end-only** — Snapdragon 855+ (Adreno 640+), Exynos 990+ (Mali-G77+). No code changes beyond a clean failure mode.
2. **Mainstream Android** — drop `descriptorBindingSampledImageUpdateAfterBind`: write the bindless texture set at level load, never mid-frame. Matches kryga's baked-lighting design (static scenes). Gains the Bifrost-era Mali slice (~10–15% of global Android).
3. **Broad Android (pre-2020)** — second renderer path with bounded `sampler2D[N]` array + push-constant index, no descriptor indexing at all. Owns two render paths forever; not recommended unless market data justifies it.

**Recommendation:** Option 2. The change cost is low (defer descriptor writes to load time), it doesn't fight the baked-lighting design, and it's the largest install-base gain per unit of work. Option 3 only if there's a concrete market reason.

### First Step: Validate Hardware

```bash
# Get Vulkan caps dump from target device
adb shell dumpsys gpu | grep -A 100 "Vulkan"
# Or use vulkaninfo APK
```

---

## Phase 1: Build System (1-2 weeks)

### 1.1 Add Android NDK Toolchain

- Create `cmake/android.toolchain.cmake`
- Target: `arm64-v8a` (drop armeabi-v7a, x86 initially)

### 1.2 Create Android Gradle Project

- `android/` directory with `build.gradle.kts`
- JNI bridge to engine
- APK packaging

### 1.3 Cross-compile Thirdparty Dependencies

- SDL2 from source (not binary download)
- Most header-only libs need no changes
- Boost: strip to header-only subset or replace Beast

### 1.4 Asset Packaging Strategy

- **Option A:** Assets in APK `assets/` folder -> VFS mount from AAssetManager
- **Option B:** Download assets on first launch -> VFS mount from internal storage

---

## Phase 2: Platform Layer (2-3 weeks)

### 2.1 Android Entry Point

- `libs/native/private/src/android/native_activity.cpp`
- JNI callbacks: onCreate, onResume, onPause, onDestroy
- Surface lifecycle management (can recreate without destroying VkDevice)

### 2.2 Windowing Approach

**SDL2 Route:**
- Use SDL_main, SDL handles Activity lifecycle
- Benefit: Less code, handles rotation/keyboard
- Drawback: Another layer, harder to debug

**Native Route:**
- Direct ANativeWindow via NativeActivity
- Benefit: Full control, smaller APK
- Drawback: More code for input/lifecycle

### 2.3 VFS Android Backend

- `libs/vfs/private/src/android_asset_backend.cpp`
- Read from APK via AAssetManager
- Write to internal storage for cache/generated

### 2.4 Input Mapping

- Touch -> Mouse emulation (single touch)
- Multi-touch -> Custom handling or ignore initially
- On-screen controls if needed

---

## Phase 3: Vulkan Adaptation (1-2 weeks)

### 3.1 Surface Creation

- `vulkan_render_device.cpp`: Add `VK_KHR_android_surface` path
- Already abstracted via SDL if using SDL2

### 3.2 Extension Negotiation

Today `vulkan_render_device.cpp:150-163` enables every descriptor-indexing sub-feature unconditionally. VkBootstrap fails opaquely on missing features. Concrete changes:

- Query `VkPhysicalDeviceDescriptorIndexingFeatures` and `VkPhysicalDeviceBufferDeviceAddressFeatures` before requesting them.
- Fail with a specific named-feature error message if any required feature is missing.
- Track a `device_capability_tier` on `render_device` so later code can branch (Tier-A: full, Tier-B: no UpdateAfterBind, Tier-C: no descriptor indexing — only if Option 3 is ever pursued).
- Drop `prefer_gpu_device_type::discrete` on Android — there's only an integrated GPU (`vulkan_render_device.cpp:137`).
- Gate `request_validation_layers(true)` behind a debug build flag (`vulkan_render_device.cpp:111`) — production APKs ship without layers.

### 3.2a Bindless Slot Cap

`KGPU_max_bindless_textures = 4096` is hardcoded (`gpu_generic_constants.h:29`). Mobile devices report `maxPerStageDescriptorUpdateAfterBindSampledImages` ranging from ~1024 (some Adreno) to 500K+. The bindless array currently grows from the front for materials and from the back for shadow maps and CSM (`kryga_render_init.cpp:496, 527`), so a smaller cap reduces shadow capacity too.

- Query the limit at init, clamp `bindless_slot_count = min(4096, device_limit)`.
- Make shadow-map back-allocation use the runtime count, not the compile-time constant.
- Log the chosen value so we can spot underprovisioning on real devices.

### 3.2b Drop UpdateAfterBind (Option 2 implementation)

Goal: remove `descriptorBindingSampledImageUpdateAfterBind` from required features.

- Audit every `vkUpdateDescriptorSets` write into the bindless set — confirm none happen after the first frame for a given level.
- Move all bindless writes to load-time, before the descriptor set is bound for rendering.
- Remove `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT` from `kryga_render_init.cpp:832` and `VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT` from line 843.
- Keep `partiallyBound` and `variableDescriptorCount`.
- If a future feature (asset hot-reload, streaming open worlds) needs mid-frame writes, restore UAB as an optional capability rather than a hard requirement.

### 3.3 Swapchain Adjustments

- `VK_PRESENT_MODE_FIFO_KHR` (vsync, battery-friendly)
- Handle pre-rotation to avoid compositor overhead
- Dynamic resolution for thermal throttling

### 3.4 Memory Budget

- VMA budget tracking for mobile constraints
- Texture streaming priority adjustments

---

## Phase 4: Mobile Optimizations (ongoing)

### 4.1 Render Quality Settings

- Resolution scaling
- Reduced shadow resolution
- Simplified lighting (fewer probes, smaller lightmaps)

### 4.2 Battery/Thermal

- Frame rate cap (30/60 selectable)
- Pause rendering when app backgrounded
- Reduce work when thermal throttling detected

### 4.3 APK Size

- Texture compression (ASTC for Android)
- Asset bundles for optional content

---

## File Changes Summary

### New Files

| Path | Purpose |
|------|---------|
| `android/` | Gradle project, Java activity |
| `cmake/android.toolchain.cmake` | NDK cross-compile |
| `libs/native/private/src/android/` | Android window/lifecycle |
| `libs/vfs/private/src/android_asset_backend.cpp` | APK asset access |
| `engine/private/src/app/android_main.cpp` | JNI entry |

### Modified Files

| Path | Changes |
|------|---------|
| `CMakeLists.txt` | Platform detection, Android target |
| `thirdparty/CMakeLists.txt` | SDL2 from source, skip Windows binaries |
| `libs/render/render/private/src/vulkan_render_device.cpp` | Optional extensions, Android surface |
| `engine/private/src/input_manager.cpp` | Touch event handling |

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| `descriptorBindingSampledImageUpdateAfterBind` missing on Bifrost-era Mali | Option 2: defer all bindless writes to load time, remove UAB flag (see Phase 3.2b) |
| `runtimeDescriptorArray` / descriptor indexing missing on pre-2020 Android | Fail with clear error; full fallback (Option 3, bounded array) only if market-justified |
| BDA (`bufferDeviceAddress` feature) missing on some mid-range Android | uvec2 path done; runtime gate + fail-clean if still missing. SSBO fallback deferred unless needed |
| Hardcoded 4096 bindless slots exceeds device limit | Clamp at init against `maxPerStageDescriptorUpdateAfterBindSampledImages` |
| Per-pixel `nonuniformEXT` sampling costs perf on TBDR | Keep material/texture index uniform per draw; audit shaders for per-pixel divergent indexing |
| SDL2 Android builds fragile | Use GameActivity (Jetpack) + native window directly |
| Large APK size from assets | Use Android App Bundles + Play Asset Delivery |
| Validation layers enabled in release builds | Gate `request_validation_layers(true)` on debug flag |

## Completed Preparatory Work

- [x] Removed unused `VK_EXT_graphics_pipeline_library` requirement
- [x] Replaced `shaderInt64` with `GL_EXT_buffer_reference_uvec2` for BDA addresses
- [x] All BDA push constants now use `bda_addr` (uvec2) type

## Deferred / Explicitly Not Doing

- **Vulkan 1.1 backport.** ABP 2022 floor is 1.1, but losing scalar block layout, native BDA path, and timeline semaphores costs more than the ~5% install base gained.
- **Dynamic rendering (`VK_KHR_dynamic_rendering`).** Coverage is narrower than traditional render passes on Android. Current render-pass code is already TBDR-friendly.
- **Compute fallback.** Compute is effectively universal on Vulkan Android; the runtime culling shaders (8×8 / 64×1 workgroups) are mobile-safe.
- **Full SSBO fallback for BDA.** Defer until a real device is found that exposes Vulkan 1.2 + descriptor indexing but lacks `bufferDeviceAddress`. uvec2 path is the agreed compromise.

---

## Current Architecture Reference

### Platform Abstraction
- SDL2 for windowing (`libs/native/`)
- VFS with mountable backends (`libs/vfs/`)

### Vulkan Setup
- VkBootstrap for instance/device creation
- VMA for memory allocation
- Requires Vulkan 1.2+

### Key Dependencies (Android-ready)
- GLM, spdlog, CLI11, cgltf, yaml-cpp (header-only or standard C++)
- SDL2 (needs NDK build)
- Boost (may need stripping)

# Android Port Plan

Created: 2026-04-14

## Overview

Port the Kryga Vulkan engine to Android. The architecture is well-suited due to clean abstraction layers (SDL2, VFS, VkBootstrap).

---

## Phase 0: Feasibility & Requirements

### Critical Vulkan Compatibility

| Vulkan Feature | Desktop | Android Support |
|----------------|---------|-----------------|
| Vulkan 1.2 | Required | 85%+ devices (API 24+) |
| ~~`VK_EXT_graphics_pipeline_library`~~ | ~~Required~~ | Removed (was unused) |
| Buffer Device Address | Required | Adreno 640+, Mali-G77+ |
| Descriptor Indexing | Required | Most Vulkan 1.2 devices |
| ~~`shaderInt64`~~ | ~~Required~~ | Removed — using uvec2 via `GL_EXT_buffer_reference_uvec2` |

### Options

1. **Target high-end devices only** - Snapdragon 855+ (Adreno 640+), Exynos 990+ (Mali-G77+)
2. **Create mobile render path** - SSBO fallback for devices without BDA/shaderInt64

**Recommendation:** Start with option 1 (high-end only), add SSBO fallback later if needed.

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

- Add runtime capability check for BDA/shaderInt64
- Fallback paths for missing features (SSBO binding if no BDA)

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
| BDA not available on low-end devices | Fall back to SSBO descriptor binding (set 1) instead of push constant BDA |
| SDL2 Android builds fragile | Use GameActivity (Jetpack) + native window directly |
| Large APK size from assets | Use Android App Bundles + Play Asset Delivery |

## Completed Preparatory Work

- [x] Removed unused `VK_EXT_graphics_pipeline_library` requirement
- [x] Replaced `shaderInt64` with `GL_EXT_buffer_reference_uvec2` for BDA addresses
- [x] All BDA push constants now use `bda_addr` (uvec2) type

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

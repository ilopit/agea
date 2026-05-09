# Vulkan Validation & Debug — Known Issues / Deferred Work

Current state (see `vulkan_render_device.cpp:init_vulkan`, `utils/vulkan_debug.h`):
- Gated by `KRYGA_VULKAN_DEBUG` (defined by CMake: `1` in Debug/RelWithDebInfo, `0` in Release/MinSizeRel). Override with `-DKRYGA_VULKAN_DEBUG=0/1` at configure time.
- When `=1`: validation layer enabled, custom `VK_EXT_debug_utils` messenger routes to spdlog (`vk_utils::debug_messenger_callback`), object names attached at creation, break-into-debugger on ERROR (Windows only).
- When `=0`: no validation layer, no messenger installed, all `KRG_VK_NAME(...)` / `KRG_VK_LABEL_*(...)` / `KRG_VK_DBG_NAME(...)` macros compile to no-ops — including the string-concat expressions inside them.
- Dedupe on `messageIdNumber + fnv1a(pMessage[:192])` for non-errors; errors always log.

**Adoption rule**: never call `vk_utils::set_debug_name(...)` or `vk_utils::cmd_*_label(...)` directly from new code. Use the macros (`KRG_VK_NAME`, `KRG_VK_LABEL_BEGIN/END/INSERT`, `KRG_VK_SCOPED_LABEL`, `KRG_VK_QUEUE_LABEL_BEGIN/END`). For passing a name through a function argument (e.g. the `debug_name` param of `vulkan_buffer::create`), wrap the name expression in `KRG_VK_DBG_NAME(...)` so the concat is stripped in Release.

The items below are deliberately NOT enabled. They have real cost or interact badly with features in use.

## 1. `VK_EXT_validation_features` extras [not enabled]

vk-bootstrap exposes `add_validation_feature_enable(VkValidationFeatureEnableEXT)`. The interesting ones:

### GPU-Assisted Validation (`VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT`)
Catches out-of-bounds descriptor indexing, bad device addresses, etc. *at GPU execution time*.

**Why deferred**: Known friction with the features this codebase relies on:
- Buffer Device Address (`VK_KHR_buffer_device_address` — enabled, used heavily via uvec2 BDA for mobile compatibility). GPU-AV instruments BDA access and historically has had false positives with `GL_EXT_buffer_reference_uvec2`.
- Descriptor indexing / partially-bound / update-after-bind (all enabled). GPU-AV has a per-draw shader-instrumentation cost that compounds with bindless.
- Real frame-time hit (~2-10x depending on scene).

**How to enable later (opt-in)**:
```cpp
builder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
builder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
```
Gate behind a CLI flag (`--vk-gpu-av`).

### Synchronization Validation (`VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`)
Detects WAR/WAW/RAW hazards across submits, barriers, subpass dependencies, render-pass-external access.

**Why deferred**: Doubles CPU validation cost. Very useful but noisy with the current render graph structure — the fix rate needs to match to avoid the log turning into wallpaper. Should be enabled during barrier/render-graph work, not always-on.

### Best Practices (`VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT`)
Fires on vendor-specific anti-patterns (NV, AMD, ARM, IMG). Overlaps partially with the Android/mobile work.

**Why deferred**: High signal on mobile but low on desktop, and emits a lot of advice-level messages that aren't actionable mid-feature-work. Recommend enabling *only* during mobile perf passes.

### Debug Printf (`VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT`)
`debugPrintfEXT(...)` from GLSL prints through the debug messenger callback.

**Why deferred**: Mutually exclusive with GPU-AV in older SDKs. Requires shaders compiled with `GL_EXT_debug_printf` and the extension declared in GLSL. Worth adding with a dedicated debug shader-include helper, not scattered one-offs.

## 2. Object naming at creation sites [done for core + loader paths]

Named today:
- Device, graphics queue, swapchain images/views
- Per-frame buffers in `kryga_render_init.cpp` (`frame_N.objects`, `frame_N.materials`, ...)
- Per-frame command pools, command buffers, fences, semaphores, and upload context fence/pool
- Render passes, depth images — via `render_pass_builder::set_debug_name(...)`
- Shader effects — vertex/fragment `VkShaderModule`, per-set `VkDescriptorSetLayout`, `VkPipelineLayout`, main + stencil `VkPipeline`, all prefixed with the effect id
- Compute shaders — same pattern, prefixed with the compute shader id
- Bindless pool / layout / set, static samplers (`sampler.linear_repeat`, ...)

**Still unnamed** (lower value, mostly short-lived or cached-shared):
- Per-frame descriptor sets allocated via `descriptor_allocator::allocate`. These reset every frame; naming them individually is noise.
- Cached `VkDescriptorSetLayout`s from `descriptor_layout_cache`. Shared across many consumers — one name wouldn't describe the shape.
- Framebuffers inside `render_pass_builder` (not currently surfaced in validation output).
- `bake/lightmap_baker.cpp` intermediate buffers/images (transient per-bake).
- UI vertex/index buffers in `kryga_render_render.cpp` (use `vulkan_buffer::create` directly; the API now accepts a name — they just don't pass one).

## 3. `VK_EXT_layer_settings` / `vk_layer_settings.txt` [not used]

Allows runtime layer config (enable printf, mute specific VUIDs, switch message format to JSON) without rebuilding.

**Why deferred**: One more config surface; only pays off when tuning validation regularly. If we start muting VUIDs from code via `VkLayerSettingsCreateInfoEXT` (pNext on instance), store the mute list in source so it's reviewable, not in a user-local settings file.

## 4. Break-on-error [enabled, Windows only]

`__debugbreak()` fires on ERROR-severity messages when `IsDebuggerPresent()`. Non-Windows platforms get nothing.

**Known gap**: no Linux/macOS equivalent (`__builtin_trap()` would work but is less friendly — aborts the process if no debugger). Android-side equivalent is `raise(SIGTRAP)` guarded by a debuggable-process check. Add if/when the Android port goes live.

## 5. Dedupe can hide genuinely new instances of the same VUID [minor]

Keyed on `(messageIdNumber << 32) ^ fnv1a(pMessage[:192])`. A VUID hit with *different* object handles but an identical message prefix (common: generic VUIDs that format the same way) will only log once per run.

**Mitigation**: call `vk_utils::debug_reset_dedupe()` at scene boundaries or frame `N` milestones. Currently never called.

## 6. Validation is always-on in all builds [intentional for now]

No `NDEBUG`/Release guard around `request_validation_layers(true)`. Validation layer adds measurable CPU overhead (~5-15%) even without extras.

**Decision**: leave on until mobile/Android branch lands, then gate behind `-r` release builds.

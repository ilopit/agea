# CLAUDE.md

## Communication

When responding to ideas or proposals:
- Lead with analysis, no agreement
- Present alternative points of view, approaches. DO NOT GIVE YOUR assessment
- Be fucking honest! If approach or idea has drawback mention it!
- Never ever use phrases like "Your are absolute right", "Excelent point" or similar bullshit
- Do what you was requested to do! Do not write tests or documentation that you were not requested to do!
- Always think what can go wrong! Think about consequences. Point to alternatives!
- You are colleague not a "yes-man". Think and help!
- Use templates like "Approach X has following benefits, but consider Y"
- Template: "Approach X benefits: ..., but consider Y"

### Feedback (after every response, about the user's request)
- **Score:** 1-10 (how clear/well-scoped the request was) | **Good:** one positive | **Bad:** one suggestion to improve the request

## Build & Run — use `/run` skill, build via `tools/build.sh`
- Build: `tools/build.sh [options] [target]` — `-v` verbose, `-r` release, `-j N` jobs, `-a` all targets. Default: `kryga_editor` Debug (desktop). On Android the only target is `kryga_game`.
- Targets:
  - `kryga_editor` — full editor: ImGui UI, gizmos, console, profiler overlay, sync service, runtime play-mode toggle (F5/Esc).
  - `kryga_game` — clean game: no ImGui, no editor UI, no sync service. ~1.9 MB smaller than editor on desktop.
  - `kryga_cook` — content cooker (editor target only — produces `build/cooked/`).
- Run: `tools/run.sh [-r] <exe> [args]` — runs from `build/project_<Config>/bin/`. Never `cd` to bin manually.
- Reconfigure: `cmake --preset host` (NOT `cmake -S . -B build`). `tools/build.sh` only auto-configures on first run; CMake `file(GLOB)` for sources means adding/renaming a `.cpp` requires a manual reconfigure or the build fails with "cannot open source file" pointing at the old path.
- Visual regression: `tools/run.sh visual_regression_tests.exe`, update refs with `UPDATE_REFERENCES=1 tools/run.sh visual_regression_tests.exe`

## Structure
```
engine/          Main app
libs/            Internal libs (see below)
packages/        Game modules — 3 layers: <name>.model, <name>.render, <name>.builder
resources/       Runtime data, shaders, .apkg packages
  shaders_includes/  Shared GLSL (common_vert.glsl, common_frag.glsl)
  packages/base.apkg/class/shader_effects/  System/UI shaders
docs/            Design docs & issue tracking
  plans/         Design docs (vision, game design, engine roadmap, milestones) — see docs/plans/README.md
  issues/        Per-subsystem known issues & tech debt (meshes, shadows, validation, editor, packages)
tools/           Scripts (argen.py reflection generator)
thirdparty/      NEVER modify (except thirdparty/unofficial, thirdparty/CMakeLists.txt)
cmake/           CMake scripts
```

### Key libs/
- `core` — reflection registry, package/level managers, object caches, archetypes
- `global_state` — singleton state: `kryga::glob::glob_state()`, lifecycle: create→connect→init→ready
- `render/` — Vulkan subsystem: `gpu_types` (shared C++/GLSL headers), `shader_system`, `render`, `utils`
- `render_bridge` — connects model objects to Vulkan render data
- `ar` — reflection macros (`KRG_ar_*`) parsed by argen.py → `build/kryga_generated/`
- `utils` — id, path, buffer, allocators, singletons, logging
- `animation` — ozz-based skeletal animation: blending layers, two-bone/aim IK, per-instance playback
- `assets_importer` — imports meshes (OBJ→amsh) and textures (image→atxt), UV2 generation via xatlas
- `asset_converter_v2` — headless glTF/OBJ→engine converter: parses scenes, emits packages and levels
- `cook` — content cooker: compiles shaders to SPIR-V via glslc, rewrites aobj descriptors, copies assets
- `serialization` — YAML-based read/write of data containers via vfs paths
- `vfs` — virtual file system: mountable backends (physical, memory, manifest, Android APK), rid-based I/O
- `project_paths` — discovers repo/staged layout by walking up from exe to `kryga.project` anchor file
- `kryga_port` — header-only platform portability shims: OS detection macros, `<format>` polyfill, ImGui gate
- `native` — SDL window creation and management
- `rpc` — JSON-RPC 2.0 server over localhost TCP (LSP framing) with spdlog notification sink
- `error_handling` — engine-wide `result_code` enum (ok, failed, serialization_error, etc.)
- `testing` — GTest base fixtures with engine workspace setup/teardown

### Renderer internals
- Shader effects created on render_pass via `render_pass::create_shader_effect()`
- Materials via `vulkan_render_loader::create_material()`
- System meshes: `plane_mesh` (fullscreen quad), accessed via `vulkan_render_loader::get_mesh_data()`
- Camera UBO (set 0): projection, inv_projection, view, position
- Push constants: material_id, texture_indices, sampler_indices, instance_base, light ids
- Descriptor sets: 0=camera, 1=objects/lights/clusters, 2=bindless textures+samplers, 3=material

## Baked Lighting

### Overview
Baked lighting provides pre-computed GI for static geometry via lightmaps and SH light probes for dynamic objects. Realtime lights (direct + specular + shadows) still work on top of baked data.

### Architecture
```
vertex_data.uv2          Lightmap UV channel (location 4, auto-generated by xatlas on import)
object_data.lightmap_scale/offset/texture_index  Per-instance lightmap binding
KRYGA_LIGHTMAPPED / KRYGA_SKINNED   Preprocessor guards in common_vert/frag.glsl
```

### Key files
| Area | Files |
|------|-------|
| Vertex format | `gpu_types/gpu_vertex_types.h` (`vertex_data.uv2`), `gpu_types.cpp` (layout) |
| Per-instance binding | `gpu_object_types.h` (`lightmap_scale/offset/texture_index`) |
| Shaders | `common_vert.glsl` / `common_frag.glsl` (ifdef guards), `lightmap_sampling.glsl`, `se_lit_lightmapped.vert/.frag` |
| UV2 generation | `assets_importer/uv2_generator.h/.cpp` (xatlas wrapper), `assets_importer.cpp` (auto-runs on import) |
| Atlas packing | `vulkan_render/lightmap_atlas.h/.cpp` (skyline algorithm) |
| BVH | `gpu_bvh_types.h`, `bake/bvh_builder.h/.cpp` (CPU SAH builder) |
| Compute shaders | `bake/gbuffer_rasterize.comp`, `lightmap_baker_direct.comp`, `lightmap_baker_indirect.comp`, `ao_baker.comp`, `lightmap_denoise.comp` |
| Baker | `bake/lightmap_baker.h/.cpp` (orchestrates full GPU bake pipeline via `immediate_submit`) |
| Probes | `gpu_probe_types.h` (SH L2), `bake/probe_placer.h/.cpp`, `bake/probe_baker.comp`, `bake/sh_common.glsl`, `probe_lighting.glsl` |
| Runtime | `kryga_render.h` (`frame_buffers.probe_data/probe_grid`), `object_data.probe_index/.lightmap_scale/.lightmap_offset/.lightmap_texture_index` |

### How to use baked lighting

**1. Assign a lightmapped shader to a material**
Use `se_lit_lightmapped.vert` / `se_lit_lightmapped.frag` as the shader effect. These define `KRYGA_LIGHTMAPPED` before including common headers, which enables UV2 pass-through and lightmap sampling.

**2. Assign a lightmap texture (per-instance)**
Lightmap binding is per-instance, not per-material. Set `object_data.lightmap_texture_index` to the bindless texture index of the lightmap atlas. Set `object_data.lightmap_scale` and `object_data.lightmap_offset` to remap mesh UV2 into this instance's atlas sub-region. The vertex shader applies `uv_out = uv_in * lightmap_scale + lightmap_offset`. Defaults: `lightmap_scale = (1,1)`, `lightmap_offset = (0,0)`, `lightmap_texture_index = 0xFFFFFFFF` (none). Sampler is hardcoded to `KGPU_SAMPLER_LINEAR_CLAMP` (1).

**3. UV2 generation**
UV2 is auto-generated by xatlas during mesh import (`convert_3do_to_amsh`). For manual control, call `uv2_generator::generate_uv2()` directly. xatlas may split vertices at UV seams — the output vertex/index buffers can be larger than input.

**4. Bake lightmaps (GPU compute)**
```cpp
render::lightmap_baker baker;
baker.add_mesh(vertices, vert_count, indices, idx_count);

render::bake::bake_settings settings;
settings.resolution = 1024;
settings.samples_per_texel = 64;
settings.bounce_count = 2;

auto result = baker.bake(settings);
// result.success, baker.get_lightmap_data() (RGBA16F), baker.get_ao_data() (R16F)
```
The baker runs: BVH build (CPU) → G-buffer rasterize → direct light → indirect bounces → AO → denoise, all via `immediate_submit`.

**5. Light probes for dynamic objects**
Probes are placed on a uniform grid via `probe_placer::place_probes_grid()`. After baking, probe SH coefficients are uploaded to the `probe_data` SSBO. Each dynamic object's `object_data.probe_index` is set per-frame to the nearest probe. Fragment shaders evaluate SH via `probe_lighting.glsl` (`evaluate_probe_lighting()` or `evaluate_probe_lighting_interpolated()`).

### Instancing and lightmaps
Lightmap binding is per-instance via `object_data` fields, not per-material. Each instance of the same mesh can have its own atlas region (`lightmap_scale` + `lightmap_offset`) and texture (`lightmap_texture_index`). This allows instanced draw calls (`vkCmdDrawIndexed` with `instance_count > 1`) where each instance samples its own baked lighting. The vertex shader reads `lightmap_scale`/`lightmap_offset` from the instance's `object_data` and transforms `in_lightmap_uv` accordingly.

### Shader blending model
- **Lightmapped static**: `baked_gi * albedo + realtime_direct` (lightmap replaces ambient, realtime adds direct diffuse + specular with shadows)
- **Probe-lit dynamic**: `probe_sh(normal) * albedo + realtime_direct` (same pattern, probes replace ambient)
- **Non-baked**: unchanged (`ambient + diffuse + specular` all realtime)

### Descriptor set 1 bindings
Bindings 9-10 added for probes (all shaders declare them for layout compatibility):
- Binding 9: `ProbeDataBuffer` (sh_probe array)
- Binding 10: `ProbeGridBuffer` (probe_grid_config)

### Gotchas
- `skinned_vertex_data` has NO `uv2` — skinned meshes use probes, not lightmaps. `KRYGA_SKINNED` guard prevents the UV2 input declaration.
- All shaders consuming `vertex_data` must declare `layout(location=4) in vec2 in_lightmap_uv` even if unused — needed for correct vertex stride (52 bytes).
- Lightmap texture is bound per-instance via `object_data.lightmap_texture_index`, NOT per-material. `pbr_material` has no lightmap slot.
- Compute shaders are compiled at runtime via glslc.exe, not at build time.

## Engine Roadmap

Full design docs in `docs/plans/`, known issues in `docs/issues/`.

### Key platform decisions
- **Render API:** Vulkan 1.2 + BDA only, no GL/ES fallback
- **Android target:** API 30+, arm64-v8a, Adreno 640+ / Mali-G77+. iOS deferred.
- **BDA addresses:** `uvec2` via `GL_EXT_buffer_reference_uvec2` (not `uint64_t` / `shaderInt64`)

### Subsystem priorities (Doc 04)
- **P0 (build):** audio (miniaudio), player-facing UI (custom minimal), save system, touch+gestures, app state machine, event bus, Android build pipeline
- **Keep as-is:** Vulkan core, baked lighting, shadows, clustered lights, glTF import, ImGui editor, scene/package system, `.apkg`+VFS, reflection
- **Defer:** dynamic resolution scaling, post-processing, cloud save, hot-reload, analytics/ads/IAP SDKs
- **Cut:** `packages/tbs` (hex grid), DPSM, GPU particles

### Known issues & tech debt
Tracked in `docs/issues/` per subsystem: meshes, shadows, validation, editor UI state, package organization.

## Rules
- NEVER change dependency versions
- Do not read `*.ar.cpp` generated files without permission
- C++23, static libraries, Vulkan
- No defensive null-checks / early-returns — use asserts (`KRG_check`) instead. If something should never be null, assert on it; don't silently skip.
- NEVER run `UPDATE_REFERENCES=1` for visual regression tests without explicit user confirmation. When tests fail, report which tests failed and WHY the output differs before proposing a reference update.

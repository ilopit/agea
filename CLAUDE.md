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

### Feedback (after every response)
- **Score:** 1-10 | **Good:** one positive | **Bad:** one improvement

## Build (Windows: `cmd //c tools\build.bat`, MSYS: `tools/build.sh`)
Options: `-v` verbose, `-c` clean, `-r` release, `-j N` jobs, `-a` all targets
Target: `tools/build.bat <name>` (e.g. `engine`, `utils_tests`, `shader_system_tests`)
Tests run from: `build/project_Debug/bin/`

## Structure
```
engine/          Main app
libs/            Internal libs (see below)
packages/        Game modules — 3 layers: <name>.model, <name>.render, <name>.builder
resources/       Runtime data, shaders, .apkg packages
  shaders_includes/  Shared GLSL (common_vert.glsl, common_frag.glsl)
  packages/base.apkg/class/shader_effects/  System/UI shaders
tools/           Scripts (argen.py reflection generator)
thirdparty/      NEVER modify (except thirdparty/unofficial, thirdparty/CMakeLists.txt)
cmake/           CMake scripts
```

### Key libs/
- `core` — reflection registry, package/level managers, object caches, archetypes
- `global_state` — singleton state: `kryga::glob::glob_state()`, lifecycle: create→connect→init→ready
- `render/` — Vulkan subsystem: `gpu_types` (shared C++/GLSL headers), `shader_system`, `render`, `utils`
- `render_bridge` — connects model objects to Vulkan render data
- `resource_locator` — resolves paths by category (assets, shaders, levels)
- `ar` — reflection macros (`KRG_ar_*`) parsed by argen.py → `build/kryga_generated/`
- `utils` — id, path, buffer, allocators, singletons, logging

### Renderer internals
- Shader effects created on render_pass via `render_pass::create_shader_effect()`
- Materials via `vulkan_render_loader::create_material()`
- System meshes: `plane_mesh` (fullscreen quad), accessed via `vulkan_render_loader::get_mesh_data()`
- Camera UBO (set 0): projection, inv_projection, view, position
- Push constants: material_id, texture_indices, sampler_indices, instance_base, light ids
- Descriptor sets: 0=camera, 1=objects/lights/clusters, 2=bindless textures+samplers, 3=material

## Rules
- NEVER change dependency versions
- Do not read `*.ar.cpp` generated files without permission
- C++23, static libraries, Vulkan

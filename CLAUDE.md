# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Communication Guidelines

When responding to ideas or proposals:
- Lead with analysis, no agreement
- Present alternative points of view, approaches. DO NOT GIVE YOUR assessment
- Be fucking honest! If approach or idea has drawback mention it!
- Never ever use phrases like "Your are absolute right", "Excelent point" or similar bullshit
- Do what you was requested to do! Do not write tests or documentation that you were not requested to do!
- Always think what can go wrong! Think about consequences. Point to alternatives!
- You are colleague not a "yes-man". Think and help!
- Use templates like "Approach X has following benefits, but consider Y"

### Developer Feedback Loop
After every user request or communication, provide:
- **Score:** 1-10 rating for the request/communication quality
- **Good:** One positive aspect
- **Bad:** One area for improvement

## Build Commands
```bash
tools/configure.sh      # or: tools\configure.bat (Windows cmd)
tools/build.sh          # or: tools\build.bat (Windows cmd)
```
Options: `-v` verbose, `-c` clean (configure), `-r` release, `-j N` parallel jobs, `-a` all targets (build)

Build specific target: `tools/build.sh <target_name>`

### Test Targets
- `utils_tests` - Utils library tests
- `model_tests` - Core model tests
- `render_bridge_tests` - Render bridge tests
- `render_utils_tests` - Render utilities tests
- `vulkan_render_tests` - Vulkan render tests
- `shader_system_tests` - Shader system tests

Run a single test: `tools/build.sh utils_tests && ./build/project_Debug/bin/utils_tests`

## Repository Structure
- `engine/` - Main engine application
- `libs/` - Internal libraries (see below)
- `packages/` - Semi-pluggable game modules (static_mesh, lights, etc.) - compiled code that pairs with .apkg data in resources/
- `resources/` - Runtime data: levels, objects, configs, shaders, fonts, packages (.apkg files)
- `tools/` - Python scripts including argen.py (reflection generator, invoked by cmake)
- `thirdparty/` - External dependencies. **Never modify** except `thirdparty/unofficial` and `thirdparty/CMakeLists.txt`
- `cmake/` - CMake extension scripts

### libs/ Structure
- `ar` - Reflection macros (KRG_ar_class, KRG_ar_property, etc.) - parsed by argen.py
- `assets_importer` - Converts external assets to kryga formats (.amsh meshes, .atxt textures)
- `core` - Central engine systems: reflection registry, package/level managers, object caches, architypes
- `error_handling` - Result codes (ok, failed, serialization_error, etc.)
- `global_state` - Global singleton state holding caches, managers, resource_locator. Access via `kryga::glob::glob_state()`
- `native` - SDL window wrapper
- `render/` - Vulkan renderer subsystem (gpu_types, shader_system, render, utils)
- `render_bridge` - Connects model objects to Vulkan render data, manages render dependencies
- `resource_locator` - Resolves paths by category (assets, shaders, levels, etc.)
- `serialization` - YAML-based read/write for containers
- `testing` - Test utilities wrapping gtest
- `utils` - Common utilities: id, path, buffer, allocators, singletons, logging

## Architecture

### Reflection System
argen.py parses C++ headers with `KRG_ar_*` macros and generates reflection code:
- Macros: `KRG_ar_class`, `KRG_ar_property`, `KRG_ar_type`, `KRG_ar_struct`, `KRG_ar_function`, `KRG_ar_ctor`, `KRG_ar_package`, `KRG_ar_model_overrides`, `KRG_ar_render_overrides`
- Generated files go to `build/kryga_generated/`
- Each package has a `public/ar/config` file listing headers to process

### Package Structure
Packages have three layers built as separate static libraries:
1. `packages.<name>.model` - Object model with reflection
2. `packages.<name>.render` - Render-specific code
3. `packages.<name>.builder` - Asset building utilities

### Global State
Access via `kryga::glob::glob_state()`. State has lifecycle stages: create → connect → init → ready. Key components:
- Class/instance caches (objects, components, materials, meshes, textures, shader_effects)
- Managers: level_manager, package_manager, reflection_type_registry, lua_api

### Type Loading Stages
1. Make glue type IDs
2. Make type resolver
3. Handle packages in topoorder:
   - Create Model types → RT type (assign handlers, architecture, parent)
   - Create type properties (after all types exist to avoid dependencies)
   - Create Render (assign render type overrides and properties)
   - Finalize type (inherit handlers and architecture)

## Important Rules
- NEVER change dependency versions
- Use `tools/build.sh` and `tools/configure.sh` - they suppress verbose output by default
- Do not read generated `*.ar.cpp` files without permission (they are large)
- C++23 standard, static libraries, Vulkan required

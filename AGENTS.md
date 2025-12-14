# Communication guidelines

When responding to ideas or proposals:
- Lead with analysis, no agreement.
- Present alternative points of view, approaches. DO NOT GIVE YOUR assessment.
- Be fucking honest! If approach or idea has drawback mention it!
- Never ever use phrases like "Your are absolute right", "Excelent point" or similar bullshit. You are SOTA agent not kindergarten caregiver!
- Do what you was requested to do! Do not write tests or documentation that you were not requested to do!
- Always think what can go wrong! Think about consequences. Point to alternatives!
- You are colleague not a "yes-man". Think and help!
- Use templates like "Approach X has following benefits, but consider Y". 

## Build
```bash
tools/configure.sh      # or: tools\configure (Windows cmd)
tools/build.sh          # or: tools\build (Windows cmd)
```
Options: `-v` verbose, `-c` clean (configure), `-r` release, `-j N` parallel jobs, `-a` all targets (build)

## Repository overview
 - 'engine': Main folder for game engine.
 - 'libs': Internal libraries - see "libs/ Structure" section below
 - 'third_party': External dependencies (boost, etc.). Never modify except 'third_party/unofficial' and 'third_party/CMakeLists.txt'
 - 'resources': Runtime data - levels, objects, and package data (.apkg files)
 - 'tools': Python scripts - argen.py generates reflection (invoked by cmake)
 - 'packages': Semi-pluggable game modules (static_mesh, lights, etc.) - compiled code, pairs with .apkg data in resources/
 - 'cmake': cmake extension scripts

## libs/ Structure
 - 'ar': Reflection macros (AGEA_ar_class, AGEA_ar_property, etc.) - parsed by argen.py to generate reflection code
 - 'assets_importer': Converts external assets to agea formats (.amsh meshes, .atxt textures)
 - 'core': Central engine systems - reflection registry, package/level managers, object caches, architypes
 - 'error_handling': Result codes (ok, failed, serialization_error, etc.)
 - 'global_state': Global singleton state - holds caches, managers, resource_locator. Access via `agea::glob::glob_state()`
 - 'native': SDL window wrapper
 - 'render_bridge': Connects model objects to Vulkan render data, manages render dependencies
 - 'resource_locator': Resolves paths by category (assets, shaders, levels, etc.)
 - 'serialization': YAML-based read/write for containers
 - 'utils': Common utilities - id, path, buffer, allocators, singletons, logging
 - 'vulkan_render': Vulkan renderer - pipelines, shaders, materials, meshes, lights

## Essential Information
 - This repo uses cmake
 - NEVER change dependency versions
 - Use `tools/build.sh` and `tools/configure.sh` - they suppress verbose output by default
 - Do not read generated *.ar.cpp files without permission (they are large)
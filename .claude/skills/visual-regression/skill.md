---
name: visual-regression
description: Work with visual regression tests — run, update references, add new tests. Use when the user mentions visual tests, regression tests, reference images, or rendering tests.
argument-hint: "[run|update|check|add] [test_filter]"
allowed-tools: Bash, Read, Edit, Write, Grep, Glob
---

Visual regression tests live in:
- **Test file:** `libs/render/render/private/tests/visual/test_visual_regression.cpp`
- **References:** `build/project_Debug/test_references/*.png` (symlinked from `resources/test_references/`)
- **Diff output:** `build/project_Debug/tmp/test_output/`

## Commands

### Run all tests (verify against references)
```
tools/run.sh visual_regression_tests.exe
```

### Run specific test
```
tools/run.sh visual_regression_tests.exe --gtest_filter="visual_pipeline_test.lit_cubes"
```

### Update references (regenerate PNGs)
```
UPDATE_REFERENCES=1 tools/run.sh visual_regression_tests.exe
```

### Update specific test reference only
```
UPDATE_REFERENCES=1 tools/run.sh visual_regression_tests.exe --gtest_filter="visual_pipeline_test.alpha_blending"
```

## CRITICAL: NEVER regenerate references

**Claude MUST NEVER run `UPDATE_REFERENCES=1`.** Reference images are maintained by the user only. If a test fails:

1. **Run WITHOUT `UPDATE_REFERENCES`** — see which tests fail and the diff details
2. **Report the failure** to the user with pixel diff %, SSIM, and which tests failed
3. **Investigate the root cause** — is it a real bug or an expected change?
4. **Fix the code**, not the reference
5. **Ask the user** to regenerate references if the change is intentional

The user will run `UPDATE_REFERENCES=1` themselves when they decide the new output is correct. Blindly regenerating hides regressions.

## Build

```
tools/build.sh visual_regression_tests
```

If GPU struct layout changes (e.g. adding material fields), build `engine_app` first to trigger argen:
```
tools/build.sh engine_app && tools/build.sh visual_regression_tests
```

## Adding a new test

Tests use `visual_pipeline_test` fixture which provides:

### Mesh helpers
- `create_cube_mesh(id, color)` — unit cube
- `create_plane_mesh(id, color, size)` — XZ ground plane
- `create_sphere_mesh(id, color, stacks, slices, radius)` — UV sphere

### Shader helpers
- `create_lit_shader_effect(id)` — loads `se_solid_color_lit` shaders
- `create_unlit_shader_effect(id)` — loads `se_solid_color` shaders (no lighting)
- `create_transparent_shader_effect(id)` — loads `se_solid_color_lit_alpha` with `alpha_mode::world`

### Material helper
```cpp
make_solid_color_gpu_data(ambient, diffuse, specular, shininess, opacity)
// opacity defaults to 1.0
```

### Scene setup helper
```cpp
setup_scene(prefix, eye, center, shader_effect, enable_shadows)
// Creates: camera, directional light, floor plane
// prefix: unique string to avoid ID collisions between tests
```

### Scheduling pattern
```cpp
auto* obj = cache.objects.alloc(AID("unique_name"));
loader.update_object(*obj, *mat, *mesh, model, normal_matrix, position);
renderer.schedule_to_drawing(obj);
renderer.schedule_game_data_gpu_upload(obj);
renderer.schedule_material_data_gpu_upload(mat);
```

### Transparent objects
```cpp
obj->queue_id = "transparent";
```

### Outlined objects
```cpp
obj->outlined = true;
```

### Lights
```cpp
// Point light
auto* pl = cache.universal_lights.alloc(AID("name"), light_type::point);
pl->gpu_data.position = {x, y, z};
pl->gpu_data.diffuse = {r, g, b};
pl->gpu_data.radius = 8.0f;
pl->gpu_data.type = KGPU_light_type_point;
pl->gpu_data.cut_off = -1.0f;
pl->gpu_data.outer_cut_off = -1.0f;
renderer.schedule_universal_light_data_gpu_upload(pl);

// Spot light
auto* sp = cache.universal_lights.alloc(AID("name"), light_type::spot);
sp->gpu_data.position = {x, y, z};
sp->gpu_data.direction = glm::normalize(dir);
sp->gpu_data.type = KGPU_light_type_spot;
sp->gpu_data.cut_off = std::cos(glm::radians(inner_deg));
sp->gpu_data.outer_cut_off = std::cos(glm::radians(outer_deg));
renderer.schedule_universal_light_data_gpu_upload(sp);
```

### ID uniqueness
All IDs (meshes, materials, objects, lights) MUST be unique across all tests — `combined_pool::alloc` returns nullptr on duplicates. Use a test-specific prefix.

### Final render + compare
```cpp
for (int i = 0; i < 3; ++i) renderer.draw_headless();  // stabilize frame state
compare("test_name", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
```

Use 5 frames instead of 3 when shadows are enabled.

## Comparison parameters
- Pixel tolerance: 1 per channel
- SSIM threshold: 0.99
- On failure: saves `test_output/<name>_actual.png` and `test_output/<name>_diff.png`

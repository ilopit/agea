# Visual Regression Test Plan

Gap-filling tests for `libs/render/render/private/tests/visual/test_visual_regression.cpp`.
References go in `resources/test_references/<test_name>.png`. All tests use `visual_pipeline_test` base with default 512x512 unless noted.

Naming convention: `<area>_<variant>` (e.g. `shadow_spot_basic`).

---

## Priority 1 — High-value uncovered paths

### `mesh_skinned_basic`
- **Scenario:** Single skinned mesh (humanoid or test rig with 2-bone skeleton). Bind pose held; one bone rotated 45° around Z to flex the joint. One directional light, default material via `se_simple_texture_lit_skinned`. Camera framing the bend.
- **Expected:** Mesh visibly deformed at the joint with correct lighting on rotated faces. Catches regressions in bone matrix SSBO upload, skinned vertex shader path, `KRYGA_SKINNED` shader define, and `frame_buffers.bone_matrices` binding.

### `material_textured_albedo`
- **Scenario:** Cube with `se_simple_texture_lit`, sampling a 256x256 checkerboard albedo texture (committed PNG in `data://test_textures/`). Single directional light. Camera angled so 3 faces visible.
- **Expected:** Checker pattern wraps each face with correct UV orientation; lighting modulates albedo. Catches regressions in bindless texture indexing, sampler binding, and UV0 vertex attribute.

### `material_pbr_textured`
- **Scenario:** Sphere with `se_pbr_lit`, distinct albedo + specular textures. Single directional light at 45°.
- **Expected:** Specular highlight follows specular map, albedo follows albedo map. Catches regressions in PBR fragment shader, multi-texture sampling, and material SSBO layout for `pbr_material`.

### `shadow_spot_basic`
- **Scenario:** Cube floating above a floor; single spot light from above-left with shadows enabled (replaces `spot_light` test which has shadows off). Cube positioned to cast a clear cone-clipped shadow.
- **Expected:** Sharp shadow on floor, shadow boundary aligned with spot cone. Catches regressions in spot-light shadow pass and `shadow_local_*` render passes.

### `shadow_point_dpsm`
- **Scenario:** Cube above floor + 3 walls forming a corner; point light at center. Shadows enabled.
- **Expected:** Shadows cast in all directions onto floor and both walls. Catches regressions in dual-paraboloid shadow mapping (`se_shadow_dpsm.vert`) and the `shadow_local_back_*` pass.

### `shadow_csm_cascades`
- **Scenario:** Long ground plane stretching from camera near to far (z = 0..50). Row of identical cubes at z = 2, 10, 25, 45 — spanning all 4 cascade splits. Directional light + shadows enabled.
- **Expected:** Each cube casts a sharp shadow; shadow quality consistent across cascades, no visible cascade seams. Catches PSSM split regressions and cascade selection bugs.

### `shadow_pcf_modes`
- **Scenario:** Same scene as `directional_shadows`. Render 5 sub-images side-by-side (or 5 separate tests sharing one helper) with PCF set to 3x3, 5x5, 7x7, Poisson-16, Poisson-32 respectively.
- **Expected:** Visibly softer shadow penumbra as filter widens; Poisson modes have characteristic noise pattern. Catches regressions in any PCF code path.

### `probe_lighting_basic`
- **Scenario:** Sphere (dynamic, no lightmap) sitting inside a probe grid pre-baked from a colored environment (e.g. red walls left, blue walls right). Camera shows the sphere.
- **Expected:** Sphere left side tinted red, right side blue from SH probe sampling. Catches regressions in `probe_lighting.glsl`, probe SSBO upload, `object_data.probe_index` assignment, and probe interpolation.

---

## Priority 2 — Pipeline correctness

### `culling_frustum_basic`
- **Scenario:** Ring of 8 cubes at known positions; camera rotated so 4 are out of frustum. Frustum culling enabled.
- **Expected:** Only 4 cubes visible. Render with culling on vs off should match pixel-for-pixel for the visible cubes. Validates GPU compute frustum cull.

### `culling_cluster_many_lights`
- **Scenario:** 32 small point lights arranged in a 4x8 grid above a flat floor. Each light a different color, radius 2.0 (overlapping clusters).
- **Expected:** Distinct colored pools on floor; light contributions not clipped or duplicated. Stresses cluster cull compute and `max_lights_per_cluster`.

### `mesh_instanced_draw`
- **Scenario:** 100 instances of a single cube mesh on a 10x10 grid, one instanced draw call. Single directional light. Solid color material.
- **Expected:** All 100 cubes visible with consistent shading. Per-instance transform pulled from `instance_slots`. Catches regressions in `gl_InstanceIndex` indirection and instanced draw path.

### `pipeline_render_scale`
- **Scenario:** `complex_scene` reused but with `render_scale_cfg.enabled = true, divisor = 2`.
- **Expected:** Recognizably the same scene, lower internal resolution upscaled to 512x512 with nearest-neighbor (slightly blocky). Catches regressions in downsample pass + `draw_scene_upscale` + composite.

### `camera_orthographic`
- **Scenario:** Three cubes at z = 0, 5, 10 (depth ordering); orthographic projection.
- **Expected:** All three cubes equal screen-space size (no perspective foreshortening). Validates ortho projection matrix path.

---

## Priority 3 — Shader and post-process variants

### `shader_toon`
- **Scenario:** Sphere with `se_toon`, single directional light at 45°.
- **Expected:** Quantized diffuse bands (3-4 visible) and stepped specular highlight, not smooth gradient. Catches toon quantization regressions.

### `shader_unlit_textured`
- **Scenario:** Quad with `se_simple_texture` (unlit) sampling the same checker texture as `material_textured_albedo`.
- **Expected:** Flat texture display, no shading variation across surface. Validates unlit textured path.

### `material_normal_map`
- **Scenario:** Flat plane with albedo + normal map (brick or rough tileable normal). Light at glancing angle to make bumps obvious.
- **Expected:** Surface appears bumpy, highlights/shadows follow normal map detail. Skip this test if normal mapping is not yet wired up — flag in scenario notes.

### `material_emissive`
- **Scenario:** Two cubes side-by-side: left with emissive = 0, right with bright emissive. Dim ambient, no other lights.
- **Expected:** Right cube glows at full color regardless of lighting; left is dark. Catches regressions in emissive material channel.

### `material_alpha_sorting`
- **Scenario:** Three transparent quads at z = 0, 1, 2, each different color, partially overlapping in screen space.
- **Expected:** Correct back-to-front compositing — the scenario will show whether `alpha_blending` covered the depth-sort path or just trivial cases. If sorting is wrong, color order will be visibly inverted.

### `post_depth_outline`
- **Scenario:** Sphere on floor; `outline_cfg` enabled with depth-edge outline (the `se_depth_outline` post-process path, distinct from stencil outline used in `outline_rendering`).
- **Expected:** Black silhouette around sphere from depth discontinuity. Catches regressions in depth-edge detection and outline post composite.

### `debug_grid`
- **Scenario:** Empty scene with `debug_cfg.show_grid = true`. Camera looking down at slight angle.
- **Expected:** Infinite XZ grid with red X-axis, green Z-axis lines visible; grid fades with distance; depth-occluded by any geometry (verify by adding one cube).
- **Note:** Editor-only; skip if game build excludes grid.

### `debug_billboards_and_wireframe`
- **Scenario:** Scene with one point light + one spot light; `debug.light_icons = true` and `debug.light_wireframe = true`.
- **Expected:** Camera-facing icon billboards at each light position + wireframe radius/cone visualization. Validates `se_texture_billboard` and `se_debug_wire`.

### `shader_error_fallback`
- **Scenario:** Cube whose material references a non-existent or failed-to-compile shader effect.
- **Expected:** Cube renders in `se_error` magenta-checker fallback instead of crashing or rendering invisibly.

---

## Priority 4 — Bake pipeline isolation

These tests exist because `baked_lighting_scene` covers the integrated path; failures inside it are hard to localize.

### `bake_lightmap_direct_only`
- **Scenario:** 4 cubes on floor; bake with `bounce_count = 0` (direct only), no AO.
- **Expected:** Lightmap shows direct shadows from baking light, no indirect color bleed.

### `bake_lightmap_indirect`
- **Scenario:** Red wall and white floor; bake with `bounce_count = 2`.
- **Expected:** Visible red color bleed onto floor near wall — the diff vs `bake_lightmap_direct_only` isolates the indirect bounce code.

### `bake_ao_only`
- **Scenario:** 4 cubes touching a floor at corners; AO bake only, no direct/indirect.
- **Expected:** Dark AO contact shadow at each cube-floor contact. Isolates `ao_baker.comp`.

### `bake_lightmap_seams`
- **Scenario:** Mesh with chart boundaries at 90° (cube). Bake then sample at chart edges with mip filtering.
- **Expected:** No black seams visible at chart boundaries — validates `lightmap_dilate.comp`.

---

---

## Priority 5 — Render config toggle coverage

`render_config.h` exposes a large toggle surface (PCF mode × cascade count × lighting bools × render scale × outline × debug) — Cartesian product is in the millions. We use a strategy mix instead of brute force.

### Strategy A: Boolean toggle on/off pairs

For each boolean wiring flag, render a scene that exercises the flag with it on and off. Catches "flag silently does nothing" or "flag stuck on" regressions.

| Test | Toggle | Scene |
|---|---|---|
| `toggle_shadows_on` / `toggle_shadows_off` | `shadow.enabled` | `directional_shadows` scene |
| `toggle_lighting_directional_on` / `_off` | `lighting.directional` | `combined_lights` scene |
| `toggle_lighting_local_on` / `_off` | `lighting.local` | `combined_lights` scene |
| `toggle_lighting_baked_on` / `_off` | `lighting.baked` | `baked_lighting_scene` scene |
| `toggle_outline_on` / `_off` | `outline.enabled` | `outline_rendering` scene |
| `toggle_grid_on` / `_off` | `debug.show_grid` | empty scene with floor for depth occlusion |

Where `_on` matches an existing test, reuse the existing reference and only add the `_off` variant.

### Strategy B: Parameterized over single enum axis (`TEST_P`)

For discrete enum values, parameterize one scene over all values. Each parameter value gets its own reference image named `<test>_<param>.png`.

| Test suite | Param values | Scene |
|---|---|---|
| `shadow_pcf_modes` | `pcf_3x3`, `pcf_5x5`, `pcf_7x7`, `poisson_16`, `poisson_32` | `directional_shadows` scene |
| `shadow_cascade_count` | 1, 2, 3, 4 | `shadow_csm_cascades` scene (long ground) |
| `render_scale_divisor` | 1, 2, 4 | `complex_scene` scene |

Implementation pattern:
```cpp
class shadow_pcf_test : public visual_pipeline_test,
                        public testing::WithParamInterface<gpu_pcf_mode> {};
TEST_P(shadow_pcf_test, looks_correct) {
    auto cfg = current_render_config();
    cfg.shadow.pcf_mode = GetParam();
    apply_pending_render_config(cfg);
    setup_scene();
    render();
    compare("shadow_pcf_" + std::to_string(int(GetParam())), pass, w, h);
}
INSTANTIATE_TEST_SUITE_P(All, shadow_pcf_test,
    testing::Values(pcf_3x3, pcf_5x5, pcf_7x7, poisson_16, poisson_32));
```

### Strategy C: Quality presets (integration toggles)

| Test | Config |
|---|---|
| `preset_low` | shadows off, render_scale=2, no outline, no debug |
| `preset_medium` | shadows on PCF 3x3, render_scale=1, outline off |
| `preset_high` | shadows on Poisson-32, render_scale=1, outline on, debug grid on |

All three render the same `complex_scene` content. Catches preset-level interaction breakage that single-axis tests miss.

### Strategy E: No-op anti-regression

| Test | Setup | Expected |
|---|---|---|
| `noop_grid_offscreen` | `lit_cubes` scene + `debug.show_grid=true` but camera angled so grid is out of frame | identical to `lit_cubes.png` (zero diff) |
| `noop_outline_no_marked` | `lit_cubes` scene + outline post enabled but no objects flagged for outline | identical to `lit_cubes.png` |

These catch "feature accidentally affects all pixels" bugs without needing new reference images.

### Strategy D: Differential assertions — explicitly skipped

Tempting but too fragile for visual tests. Toggle effects are usually obvious enough that a reference diff is cheaper than writing histogram math.

### Test infra prerequisites

Before any of P5 lands:
- `visual_pipeline_test::SetUp()` must call `apply_pending_render_config(default_config)` to reset state. Otherwise PCF mode from test N leaks into test N+1.
- Confirm `apply_pending_render_config` handles buffer-resizing options (cluster tile size, shadow map size). If not, those toggles need full renderer recreation.
- Add a `current_render_config()` accessor or read-modify-write helper if one doesn't exist.

### Reference budget

| Strategy | Tests | New refs |
|---|---|---|
| A boolean pairs | 12 (6 pairs) | ~6 (reuse existing for `_on` where possible) |
| B parameterized | 3 suites × 3-5 params | 12 |
| C presets | 3 | 3 |
| E no-op | 2 | 0 (reuse `lit_cubes.png`) |
| **P5 total** | **20-22** | **~21** |

Combined with P1-P4 (25), full plan adds ~50 references. Allocate 1 day to running `UPDATE_REFERENCES=1` and reviewing each generated PNG for correctness.

---

## Implementation notes

- Add new tests as `TEST_F(visual_pipeline_test, <name>)` in `test_visual_regression.cpp`. The base class already handles device, render pass, readback, and reference comparison.
- Reference images are generated on first run via `UPDATE_REFERENCES=1` — but per repo policy never run that without explicit user confirmation.
- For tests requiring committed test assets (textures, skinned mesh): add to `resources/data/test_textures/` and `resources/data/test_meshes/`. Keep them small (<10 KB each).
- For tests that flip render config (`shadow_pcf_modes`, `pipeline_render_scale`): use the existing `apply_pending_render_config` mechanism rather than restarting the renderer.
- Tests that depend on features not yet implemented (e.g. normal mapping if absent) should be added with `GTEST_SKIP()` and a TODO, not silently omitted.

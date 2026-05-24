# Shadow System — Known Issues

## 1. Shadow center fixed at origin [major]

`kryga_render_shadows.cpp:258` — `shadow_center` is hardcoded to `(0,0,0)`. If the camera moves far from origin, cascades won't cover the visible area. The largest cascade radius is ~242 units, so anything beyond ~240m from origin has no directional shadow coverage.

**Alternative**: Follow camera position with texel-snapped increments (texel snapping logic already exists at line 300-307, it just needs to snap the shadow center instead of just the projection offset).

## 2. No frustum culling for shadow passes [major]

`draw_shadow_pass` (line 369) and `draw_shadow_local_pass` (line 494) iterate **all** `m_draw_batches`. No frustum culling against the light's view-projection. Every cascade renders the entire scene. Every local light renders the entire scene. With 4 CSM + up to 16 local passes, that's up to 20x the geometry drawn without any spatial rejection.

**Alternative**: Per-pass frustum cull against each cascade's ortho frustum / spot frustum / point light radius.

## 3. DPSM quality issues [medium-high]

DPSM has severe distortion artifacts near the equator (where `L.z + 1.0` approaches zero in `common_frag.glsl:368`). The projection compresses geometry near the hemisphere boundary, causing:
- Visible seams between front/back hemispheres
- Non-uniform shadow resolution — sharp near poles, blurry at equator
- PCF filtering in paraboloid UV space doesn't correspond to uniform world-space area

**Alternative**: Cube shadow maps (6 faces) give uniform resolution. More expensive but much higher quality. Or a single-face omnidirectional approach like Variance Shadow Maps with a tetrahedral warp.

## 4. Constant bias causes peter-panning and acne simultaneously [medium]

Bias at `common_frag.glsl:266` is a flat `currentDepth -= bias`. This constant bias:
- Causes acne on surfaces nearly parallel to light direction (need more bias)
- Causes peter-panning on surfaces facing the light (too much bias)

The normal bias (line 294) helps but is also constant. No slope-scaled depth bias. Vulkan supports `depthBiasConstantFactor` / `depthBiasSlopeFactor` in `VkPipelineRasterizationStateCreateInfo` which would handle this in hardware, or compute `max(bias, slopeFactor * tan(angle))` in the shader.

## 5. PCF mode is global, used for local lights too [medium]

`sampleShadowPCF` at `common_frag.glsl:231` reads `directional.pcf_mode` even when sampling local light shadow maps. Local lights have no independent PCF configuration. A 7x7 PCF on 8 local lights is 49 texture fetches x 8 = 392 shadow taps per fragment.

## 6. Cascade radius over-allocation [medium]

`kryga_render_shadows.cpp:276-279` — Radius is computed from `split_far * tan_half_fov`, but the frustum slice extends from `split_near` to `split_far`. Since the cascade center isn't at the camera, the actual bounding sphere should account for the frustum slice geometry relative to the light view. Current approach over-allocates shadow map space (radius too large), wasting resolution.

The calculation also doesn't account for the camera position offset relative to `shadow_center=origin`.

## 7. Per-frame heap allocation in `select_shadowed_lights` [low-medium]

`kryga_render_shadows.cpp:558` — `std::vector<shadow_candidate> candidates` is allocated on the heap every frame, plus `std::sort`. For a per-frame hot path, should use a fixed-size array or a pre-allocated member.

## 8. Point light view always looks at +Z [low-medium]

`kryga_render_shadows.cpp:630-632` — Point light view direction is hardcoded to `(0,0,1)`. DPSM quality depends on the view orientation relative to the camera. If most visible geometry is along a different axis, shadow quality will be poor where it matters. The view should orient toward the camera or toward the most relevant geometry.

## 9. Shadow atlas — no tile padding [low-medium]

Shadow atlas tiles have no padding between them. Bilinear filtering and PCF taps near tile edges can read a neighboring tile's depth data. In practice the impact is minimal (UV bounds check rejects most edge fragments, atlas clear fills unused space with depth=1.0), but at small tile sizes (64-128) the bleed covers a larger fraction of the tile. Fix: inset the UV remap by 2-4 texels so the PCF kernel stays inside each tile.

## 10. Shadow atlas — PCF not resolution-independent [medium]

PCF spread is `texelSize * N` (3 for Poisson16, 4 for Poisson32). At higher tile resolutions, texels are smaller so the spread covers less world space — shadows get sharper but the Poisson/grid pattern becomes visible as a checkerboard. Attempted fixes and results:

- **Per-pixel Poisson rotation (screen-space IGN)**: Broke cascade blend — different screen-space noise per cascade created visible waves at transitions.
- **Per-pixel rotation (world-space IGN)**: Still produced waves + other artifacts. World-space hash didn't produce clean variation.
- **Per-pixel grid rotation**: Same cascade boundary issues as Poisson rotation.
- **Resolution-independent spread** (`uv_scale.x / 2048.0`): Concept sound but tangled with rotation artifacts, not evaluated in isolation.
- **Wider cascade blend zone** (25%→50%): Darkened the scene — more area sampling the coarser next cascade.

Proper fix requires either temporal accumulation (TAA), PCSS (blocker-distance penumbra), or hardware `sampler2DShadow` for PCF.

## 11. Cascade blending doubles shadow sampling cost [low]

`calcDirectionalShadow` — In the blend zone (last 25% of each cascade), two cascades are sampled. With Poisson32, that's 64 texture taps in the blend zone.

## 12. No guard on spot light `outer_cut_off` acos [low]

`kryga_render_shadows.cpp:606` — `float fov = 2.0f * std::acos(light->gpu_data.outer_cut_off)`. If `outer_cut_off` exceeds valid acos range [-1, 1] due to a data error, NaN propagates through the entire shadow matrix. No guard.

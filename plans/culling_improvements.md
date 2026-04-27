# Culling Improvements

Created: 2026-04-19

## Status of base fix

Option 2 (centroid-centered bounding sphere) is implemented separately. This
plan covers the future extensions on top of it.

## Motivation

Current state after Option 2:
- Sphere is centered on geometry centroid (not mesh local origin).
- Tightness is acceptable for compact meshes, but loose for elongated ones
  (e.g. a hose, a spear, a long ramp). The sphere has to extend far enough
  to cover both ends, wasting a lot of empty volume.
- Sphere is the only volume — no second-stage AABB test catches the
  false-positive regions of the sphere.

Reference: Unreal uses `FBoxSphereBounds` (sphere AND axis-aligned box per
primitive) and runs sphere first then box.

## Phase 1 — Add AABB second-stage test

### Mesh side
- Store `m_local_aabb_min` / `m_local_aabb_max` (vec3 each) on `kryga::root::mesh`.
- Compute alongside the centroid in the existing root render handler loop
  (`packages/root/private/src/render/overrides/render_types_handlers.cpp`,
  the loop currently computing max distance).

### GPU object data
- Add `vec3 aabb_min` and `vec3 aabb_max` to `gpu::object_data`
  (`libs/render/gpu_types/public/include/gpu_types/gpu_object_types.h`).
  Store these in WORLD SPACE so the cull shader doesn't need the model
  matrix.
- Cmd builder transforms the 8 corners of the local AABB by the model
  matrix, then takes min/max → world AABB. Pass to render command.
- Trade-off: 24 bytes per object (two vec3s). Cull shader becomes a
  sphere-then-box test (sphere as cheap reject, box as tight test).

### Frustum cull shader
- File: `resources/shaders_includes/frustum_cull.comp`
- Add second pass after the existing sphere test:
  ```
  if (sphereInFrustum(center, radius)) {
      if (aabbInFrustum(aabb_min, aabb_max)) {
          mark_visible(...)
      }
  }
  ```
- AABB-vs-frustum: classic "n-vertex / p-vertex" test against each plane.

### CPU-side frustum
- Mirror the same change in `libs/render/utils/private/src/frustum.cpp`
  (or wherever `is_sphere_visible` lives) — add `is_aabb_visible` and call
  both in the three sites in `kryga_render_render.cpp` (lines around
  134, 799, 859).

### Risks / notes
- The world AABB is loose under rotation. For meshes that rotate every
  frame this is recomputed every frame — cheap on CPU (8 mat4×vec4 mults),
  negligible on GPU.
- An alternative is OBB (oriented bounding box) — store local AABB and
  rotation, test in object space. More accurate but more shader work.

## Phase 2 — Hierarchical / spatial culling

Currently every object is tested against the frustum individually. For
scenes with thousands of objects this becomes the bottleneck.

- Build a loose octree over scene objects. Each octree node stores a
  combined AABB.
- Cull entire branches at once on the CPU before dispatching the GPU
  cull pass.
- Re-insert objects on transform change (or every frame for dynamic).

Touches: scene/level manager. Likely in `libs/core/private/src/level_manager.cpp`
or a new spatial structure under `libs/render/`.

## Phase 3 — HZB occlusion culling

- Downsample the previous frame's depth buffer into a mip pyramid.
- For each primitive's bounds, sample the appropriate HZB mip and
  reject if all sampled depths are closer than the bounds' nearest
  depth.
- Catches objects hidden behind walls/terrain.

Significant work — touches the render graph (need to schedule depth
downsample after main pass), adds a new compute pass, adds occlusion
status to object data. Defer until profiling shows it's needed.

## Phase 4 — Per-meshlet culling (Nanite-style)

- Split meshes into meshlets (~64-128 triangles each).
- Each meshlet stores its own AABB + normal cone.
- Cull individual meshlets out of a single mesh.

This is essentially a re-architecture of the mesh pipeline. Out of
scope for now; would be a separate initiative.

## Out of scope

- Distance-based culling (per-primitive draw distance).
- Cull distance volumes (designer-set culling regions).
- Visibility precomputation (PVS) for static geometry.

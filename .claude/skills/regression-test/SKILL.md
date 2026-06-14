---
name: regression-test
description: Write Python regression tests that verify engine behavior via RPC. Use when the user asks to add a regression test, test engine behavior, or verify model→render pipeline correctness.
argument-hint: "<description of what to test>"
allowed-tools: Read, Edit, Write, Grep, Glob, Bash, Agent
---

Write a Python regression test in `tests/e2e/` that exercises the engine via JSON-RPC and verifies both model-layer and render-layer state.

## Architecture: Two-Layer Verification

Every regression test must verify **both** layers:

1. **Model layer** — the authoritative state (`model.*` RPCs). This is what gets saved to disk.
2. **Render layer** — the GPU-facing projection (`render.*` RPCs). This is what the player sees.

Testing only the model is insufficient — it doesn't catch render_translator propagation bugs. Testing only render is insufficient — it doesn't catch serialization/persistence bugs.

## Prefer generated Python proxies over raw RPC calls

Use the generated proxy classes from `build/kryga_generated/python/` (auto-added to `sys.path` by conftest). They provide type names in test code, making it clear which engine type is under test, and they're generated from the same reflection data the engine uses — so they stay in sync.

```python
# GOOD — proxy makes the type and operation explicit
from base import mesh_component
mc = mesh_component(engine, "hero_cube_mesh")
mc.set_mesh("plane_mesh")
ro = mc.get_render_data()

# OK for one-off calls or RPCs without a proxy
engine.call("render.object.data", {"id": "hero_cube_mesh"})
```

Fall back to `engine.call()` only for RPCs that have no generated proxy (render queries, editor commands, material edit sessions). Use `property_helpers.py` utilities for common patterns that aren't covered by proxies.

## Test file structure

```
tests/e2e/
  conftest.py          # Shared fixtures (engine session, level loading, file snapshots)
  rpc_client.py        # EngineRPC + EngineProcess (TCP JSON-RPC 2.0 client)
  assertions.py        # Reusable assertion helpers
  property_helpers.py  # Shared utilities for property round-trips and object creation
  test_<feature>.py    # One file per feature/subsystem
```

## Available RPC methods

All RPCs are registered in `engine/private/src/engine_rpc.cpp` — grep for `server.on_request("` to see the full current list. Before writing a test, read the handler for the RPC you need to understand params and return shape.

### RPC categories

- **`model.list`** — list model objects. Params: `source` (level/packages/all/package:<id>), `kind` (material/texture/mesh/sampler/shader_effect/game_object/component)
- **`model.scene.*`** — scene graph CRUD: getRoot, getChildren, create, delete, duplicate, rename
- **`model.object.property.get`** — generic property get for any reflected object (returns owners/categories/fields). Works on materials, textures, and all smart_objects.
- **`model.object.property.get_one`** — get a single property value
- **`model.object.property.set`** — set a property by owner_id + name + value. For material assignment: `set("hero_cube_mesh", "material", "mt_toon")`
- **`model.object.function.invoke`** — call reflected functions (get_position / set_position / move / etc.)
- **`model.type.meta`** — type metadata: properties, functions, schemas, hints
- **`model.component.*`** — listTypes, add
- **`model.level.*`** — list, load, save
- **`model.selection.*`** — get, set
- **`editor.material.edit`** — start material edit session, returns `{edit_id, material}`. Use `model.object.property.set` on `edit_id` to modify fields.
- **`editor.material.save`** — commit edit session changes to disk
- **`editor.material.discard`** — revert edit session, release temp object
- **`editor.material.preview`** — render material preview sphere as base64 PNG
- **`editor.camera.set`** — set editor camera position/pitch/yaw
- **`render.object.data`** — per-object GPU state (mesh, material, texture_indices, gpu_data)
- **`render.object.list`** — all render objects
- **`render.camera.data`** — camera matrices and position
- **`render.stats`** — object count, light count, texture count
- **`render.lights.data`** — light state
- **`render.config.get`** / **`render.config.set`** — render configuration (shadows, render_scale, etc.)
- **`engine.*`** — engine mode (editor/play), shutdown
- **`ping`** — engine health check

## Writing a test — step by step

### 1. Create the test file

```python
"""One-line description of what this tests."""
from .conftest import EXPECTED_RENDER_OBJECTS  # or EXPECTED_SCENE_OBJECTS
from . import assertions
```

### 2. Define constants at module level

Name the specific objects, materials, textures involved. Never hardcode IDs inline.

```python
MATERIAL_ID = "mt_toon"
RENDER_OBJECT = "hero_cube_mesh"
```

### 3. Use property_helpers.py and write file-local helpers

`tests/e2e/property_helpers.py` provides common utilities — use them instead of reimplementing:

```python
from .property_helpers import (
    get_property, set_property, assert_property_roundtrip,  # object properties
    get_material_fields, get_texture_slots, assert_material_field_roundtrip,  # materials
    material_begin_edit, material_set_field, material_save, material_discard,  # material edit session
    create_test_object, add_component, cleanup_object,  # dynamic objects
)
```

Key helpers:
- `get_property(engine, obj_id, name)` — reads a property from model.object.property.get, searches all owners
- `set_property(engine, obj_id, name, value)` — calls model.object.property.set + wait_frame
- `assert_property_roundtrip(engine, obj_id, name, value, tol=None)` — set + get + assert
- `get_material_fields(engine, mat_id)` — returns {name: value} dict for material properties via model.object.property.get
- `get_texture_slots(engine, mat_id)` — returns {slot_name: texture_id} for texture fields
- `material_begin_edit(engine, mat_id)` — starts editor.material.edit session, returns edit_id
- `material_set_field(engine, edit_id, field, value)` — sets a field on the edit instance via model.object.property.set
- `material_save(engine, mat_id)` / `material_discard(engine, mat_id)` — commit or revert
- `assert_material_field_roundtrip(engine, mat_id, field, value, save=True)` — edit + set + verify + restore

For render-layer reads or domain-specific helpers, add file-local functions:

```python
def _get_render_texture_indices(engine, obj_id=RENDER_OBJECT):
    ro = engine.call("render.object.data", {"id": obj_id})
    assertions.assert_not_pink_bug(ro, obj_id)
    return ro["material"]["texture_indices"]
```

### 4. Use a test class grouping related tests

```python
from base import mesh_component

class TestMeshSwap:

    def test_swap_mesh_updates_render(self, engine):
        mc = mesh_component(engine, HERO_MESH)

        # 1. Read baseline from BOTH layers
        original_mesh = mc.get_mesh()
        ro_before = mc.get_render_data()

        # 2. Perform the action via proxy
        mc.set_mesh(ALT_MESH)
        engine.wait_frame()

        # 3. Verify model updated
        assert mc.get_mesh() == ALT_MESH

        # 4. Verify render updated
        ro_after = mc.get_render_data()
        assert ro_after["mesh"]["id"] == ALT_MESH

        # 5. Verify no corruption (pink bug check on all objects)
        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        # 6. Restore original state
        mc.set_mesh(original_mesh)
        engine.wait_frame()
```

### 5. Always restore state

Tests run in sequence within a session against a live engine. Every test must restore whatever it changed — material fields, textures, created objects, transforms. The `conftest.py` reloads the level between test files but not between tests within a class.

For material edits, the restore pattern is:
```python
# restore
edit_id = material_begin_edit(engine, MAT_ID)
material_set_field(engine, edit_id, FIELD, original_value)
material_save(engine, MAT_ID)
```

For created objects:
```python
engine.call("model.scene.delete", {"id": created_obj_id})
```

### 6. Synchronize after state-changing operations

Call `engine.wait_frame()` after any mutation that needs to propagate to the render layer. This blocks until the engine completes a full frame cycle (including `consume_updated_render`), guaranteeing the render cache reflects the mutation. No sleep needed after read-only calls.

```python
material_save(engine, MAT_ID)  # calls editor.material.save + wait_frame
# render.object.data now reflects the save
```

NEVER use `time.sleep()` — use `engine.wait_frame()` for deterministic synchronization.

## Assertion helpers

Reusable assertions live in `tests/e2e/assertions.py` — read it before writing a test. Import as `from . import assertions`.

## Test level

The default level and expected objects are defined in `tests/e2e/conftest.py`. Read it before writing a test to know what's available. Available textures live in `resources/packages/base.apkg/class/textures/`.

## Shared fixtures (from `conftest.py`)

- `engine` (session-scoped) — `EngineRPC` instance, auto-starts editor if needed
- `load_test_level` (autouse) — reloads `simple_test` before each test file, restores modified files after

## EngineRPC convenience methods

Beyond `engine.call(method, params)`:

- `engine.invoke(obj_id, function, args=None)` — call a reflected function, returns the value directly
- `engine.get_type_meta(type_name)` — get type metadata (properties, functions, schemas)
- `engine.wait_frame(count=1)` — block until render propagation completes
- `engine.load_level_and_wait(level_id)` — load level and poll until ready

```python
# Invoke example
pos = engine.invoke("cube_a", "get_position")        # → [0, 0, 0]
engine.invoke("cube_a", "set_position", [[5, 3, 1]]) # args is a list of positional params
engine.invoke("cube_a", "move", [[1, 0, 0]])          # relative move

# Type introspection
meta = engine.get_type_meta("game_object")
funcs = meta["functions"]  # [{name, category, hint, return_type, invocable, args}, ...]
```

## Running tests

```bash
# All regression tests
python -m pytest tests/e2e/ -v

# Single file
python -m pytest tests/e2e/test_texture_swap.py -v

# Single test
python -m pytest tests/e2e/test_texture_swap.py::TestTextureSwap::test_texture_swap_updates_model_and_render -v

# Against already-running engine (skip auto-start)
python -m pytest tests/e2e/ -v --no-auto-start
```

## What to verify in every test

Ask these questions about the behavior under test. Each "yes" adds assertions to your test — not necessarily separate test methods.

1. **Does the action change model state?** → Use `get_property()` before and after, assert the expected field changed.
2. **Does the model change propagate to render?** → Read `render.object.data` before and after, assert the relevant render field (texture_indices, gpu_data.obj_pos, material.id, etc.) changed.
3. **Is the change reversible?** → If the feature has an undo/discard path, verify it reverts both model and render to the exact original state.
4. **Can it corrupt unrelated objects?** → After the action, `assert_not_pink_bug` on all `EXPECTED_RENDER_OBJECTS`.
5. **Does it survive persistence?** → If the change is saved to disk, do a `model.level.save` + `engine.load_level_and_wait()` cycle and verify state is still correct.

Questions 1+2 apply to almost every test. Questions 3–5 depend on the feature — skip what doesn't apply.

## Checklist before submitting

- [ ] Generated Python proxies used where available (fall back to `engine.call()` for RPCs without a proxy)
- [ ] Both model and render layers verified
- [ ] State restored at end of each test
- [ ] Constants defined at module level, not inline
- [ ] `property_helpers.py` utilities used where applicable
- [ ] `engine.wait_frame()` after async mutations
- [ ] Test class named `Test<Feature>`
- [ ] File named `test_<feature>.py`

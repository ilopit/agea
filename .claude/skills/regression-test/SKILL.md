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
2. **Render layer** — the GPU-facing projection (`render.state.*` RPCs). This is what the player sees.

Testing only the model is insufficient — it doesn't catch render_bridge propagation bugs. Testing only render is insufficient — it doesn't catch serialization/persistence bugs.

## Test file structure

```
tests/e2e/
  conftest.py          # Shared fixtures (engine session, level loading, file snapshots)
  rpc_client.py        # EngineRPC + EngineProcess (TCP JSON-RPC 2.0 client)
  assertions.py        # Reusable assertion helpers
  test_<feature>.py    # One file per feature/subsystem
```

## Available RPC methods

All RPCs are registered in `engine/private/src/engine_rpc.cpp` — grep for `server.on_request("` to see the full current list. Before writing a test, read the handler for the RPC you need to understand params and return shape.

### RPC categories

- **`model.scene.*`** — scene graph CRUD: getRoot, create, delete, duplicate
- **`model.transform.*`** — get/set position, rotation, scale on scene objects
- **`model.material.*`** — material inspection and edit sessions (edit → setField → save/discard), material assignment to mesh components
- **`model.texture.*`** — texture inventory (list)
- **`model.level.*`** — level load/save (load is async — use `engine.load_level_and_wait()`)
- **`model.properties.*`** — generic property get for any reflected object
- **`model.component.*`** — component type listing, add/remove components
- **`model.selection.*`** — editor selection state
- **`render.state.*`** — read-only GPU state: per-object (mesh, material, texture_indices, gpu_data), lights, camera, stats
- **`render.visibility.*`** — visibility overrides
- **`engine.*`** — engine mode (editor/play), shutdown
- **`tools.bake.*`** — lightmap baking config and execution
- **`tools.actions.*`** — async action status tracking
- **`tools.converter.*`** — asset converter
- **`editor.material.preview`** — material preview rendering
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

### 3. Write helper functions for reading state

Extract model-layer and render-layer reads into small functions. These get reused across tests in the file.

```python
def _get_material_fields(engine, mat_id=MATERIAL_ID):
    r = engine.call("model.material.get", {"id": mat_id})
    cats = r["material"]["categories"]
    props_cat = next(c for c in cats if c["name"] == "Properties")
    return {f["name"]: f["value"] for f in props_cat["fields"] if "value" in f}

def _get_render_texture_indices(engine, obj_id=RENDER_OBJECT):
    ro = engine.call("render.state.object", {"id": obj_id})
    assertions.assert_not_pink_bug(ro, obj_id)
    return ro["material"]["texture_indices"]
```

### 4. Use a test class grouping related tests

```python
class TestFeatureName:

    def test_action_updates_model_and_render(self, engine):
        # 1. Read baseline from BOTH layers
        model_before = _get_model_state(engine)
        render_before = _get_render_state(engine)

        # 2. Perform the action via model RPC
        engine.call("model.something.do", {"id": TARGET_ID, ...})
        engine.wait_frame()  # render_bridge propagation

        # 3. Verify model updated
        model_after = _get_model_state(engine)
        assert model_after["field"] == expected_value

        # 4. Verify render updated
        render_after = _get_render_state(engine)
        assert render_after != render_before  # or check specific fields

        # 5. Verify no corruption (pink bug check on all objects)
        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.state.object", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        # 6. Restore original state
        engine.call("model.something.undo", {"id": TARGET_ID, ...})
        engine.wait_frame()
```

### 5. Always restore state

Tests run in sequence within a session against a live engine. Every test must restore whatever it changed — material fields, textures, created objects, transforms. The `conftest.py` reloads the level between test files but not between tests within a class.

For material edits, the restore pattern is:
```python
# restore
engine.call("model.material.edit", {"id": MAT_ID})
engine.call("model.material.setField", {"id": MAT_ID, "field": FIELD, "value": original_value})
engine.call("model.material.save", {"id": MAT_ID})
engine.wait_frame()
```

For created objects:
```python
engine.call("model.scene.delete", {"id": created_obj_id})
```

### 6. Synchronize after state-changing operations

Call `engine.wait_frame()` after any mutation that needs to propagate to the render layer. This blocks until the engine completes a full frame cycle (including `consume_updated_render`), guaranteeing the render cache reflects the mutation. No sleep needed after read-only calls.

```python
engine.call("model.material.save", {"id": MAT_ID})
engine.wait_frame()
# render.state.object now reflects the save
```

NEVER use `time.sleep()` — use `engine.wait_frame()` for deterministic synchronization.

## Assertion helpers

Reusable assertions live in `tests/e2e/assertions.py` — read it before writing a test. Import as `from . import assertions`.

## Test level

The default level and expected objects are defined in `tests/e2e/conftest.py`. Read it before writing a test to know what's available. Available textures live in `resources/packages/base.apkg/class/textures/`.

## Shared fixtures (from `conftest.py`)

- `engine` (session-scoped) — `EngineRPC` instance, auto-starts editor if needed
- `load_test_level` (autouse) — reloads `simple_test` before each test file, restores modified files after

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

1. **Does the action change model state?** → Read model state before and after, assert the expected field changed.
2. **Does the model change propagate to render?** → Read `render.state.object` before and after, assert the relevant render field (texture_indices, obj_pos, material.id, etc.) changed.
3. **Is the change reversible?** → If the feature has an undo/discard path, verify it reverts both model and render to the exact original state.
4. **Can it corrupt unrelated objects?** → After the action, `assert_not_pink_bug` on all `EXPECTED_RENDER_OBJECTS`.
5. **Does it survive persistence?** → If the change is saved to disk, do a `model.level.save` + `engine.load_level_and_wait()` cycle and verify state is still correct.

Questions 1+2 apply to almost every test. Questions 3–5 depend on the feature — skip what doesn't apply.

## Checklist before submitting

- [ ] Both model and render layers verified
- [ ] State restored at end of each test
- [ ] Constants defined at module level, not inline
- [ ] Helper functions for repeated model/render reads
- [ ] `engine.wait_frame()` after async mutations
- [ ] Test class named `Test<Feature>`
- [ ] File named `test_<feature>.py`

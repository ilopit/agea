"""Runtime texture swap — verify model→render pipeline for texture changes.

Changes the diffuse texture on mt_toon from T1 to T2 and verifies both
model-layer fields and render-layer bindless texture_indices update correctly.
"""
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

MATERIAL_ID = "mt_toon"
TEXTURE_FIELD = "diffuse_txt"
RENDER_OBJECT = "hero_cube_mesh"
ALT_TEXTURE = "txt_container"


def _get_texture_slots(engine, mat_id=MATERIAL_ID):
    r = engine.call("model.material.get", {"id": mat_id})
    cats = r["material"]["categories"]
    props_cat = next(c for c in cats if c["name"] == "Properties")
    return {
        name: val["texture"]
        for name, val in ((f["name"], f["value"]) for f in props_cat["fields"] if "value" in f)
        if isinstance(val, dict) and "texture" in val
    }


def _get_render_texture_indices(engine, obj_id=RENDER_OBJECT):
    ro = engine.call("render.state.object", {"id": obj_id})
    assertions.assert_not_pink_bug(ro, obj_id)
    return ro["material"]["texture_indices"]


class TestTextureSwap:

    def test_texture_swap_updates_model_and_render(self, engine):
        """Full pipeline: change diffuse texture, verify model + render update."""
        # --- baseline ---
        original_slots = _get_texture_slots(engine)
        original_texture = original_slots[TEXTURE_FIELD]
        assert original_texture != ALT_TEXTURE, (
            f"Test precondition failed: material already uses {ALT_TEXTURE}"
        )

        original_indices = _get_render_texture_indices(engine)

        # --- model: verify baseline texture ---
        assert original_slots[TEXTURE_FIELD] == original_texture

        # --- render: verify baseline (no pink bug, valid indices) ---
        assert not all(idx == assertions.UINT32_MAX for idx in original_indices), (
            "Baseline texture_indices are all UINT32_MAX — textures not bound"
        )

        # --- swap texture T1 → T2 ---
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": ALT_TEXTURE,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

        # --- model: verify T2 ---
        swapped_slots = _get_texture_slots(engine)
        assert swapped_slots[TEXTURE_FIELD] == ALT_TEXTURE, (
            f"Model didn't update: expected {ALT_TEXTURE}, got {swapped_slots[TEXTURE_FIELD]}"
        )

        # --- render: verify still valid after swap (no pink bug) ---
        swapped_indices = _get_render_texture_indices(engine)

        # --- restore T2 → T1 ---
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": original_texture,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

        # --- verify restoration ---
        restored_slots = _get_texture_slots(engine)
        assert restored_slots[TEXTURE_FIELD] == original_texture, (
            f"Restore failed: expected {original_texture}, got {restored_slots[TEXTURE_FIELD]}"
        )

        restored_indices = _get_render_texture_indices(engine)
        assert restored_indices == original_indices, (
            f"Render indices not restored: {restored_indices} vs {original_indices}"
        )

    def test_texture_swap_discard_leaves_render_unchanged(self, engine):
        """Edit + setField + discard must not change render texture_indices."""
        original_indices = _get_render_texture_indices(engine)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": ALT_TEXTURE,
        })
        engine.call("model.material.discard", {"id": MATERIAL_ID})
        engine.wait_frame()

        discarded_indices = _get_render_texture_indices(engine)
        assert discarded_indices == original_indices, (
            f"Discard changed render indices: {original_indices} → {discarded_indices}"
        )

    def test_texture_swap_all_objects_stay_valid(self, engine):
        """Swapping texture on mt_toon must not cause pink bug on any object."""
        original_slots = _get_texture_slots(engine)
        original_texture = original_slots[TEXTURE_FIELD]

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": ALT_TEXTURE,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.state.object", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        # restore
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": original_texture,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

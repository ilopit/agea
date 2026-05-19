"""Material edit/save regression — the 'pink bug'.

The pink bug: model.material.save causes objects to fall back to se_error (pink shader).
Detectable via render.object.data:
  - material.id becomes "se_error"
  - texture_indices become all 0xFFFFFFFF

These tests also verify material field values survive every operation
(edit, setField, save, discard) using the model.material.get RPC.
"""
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

MATERIAL_ID = "mt_toon"


def _get_material_fields(engine, mat_id=MATERIAL_ID):
    """Return {field_name: value} for the material's Properties category."""
    r = engine.call("model.material.get", {"id": mat_id})
    cats = r["material"]["categories"]
    props_cat = next(c for c in cats if c["name"] == "Properties")
    return {f["name"]: f["value"] for f in props_cat["fields"] if "value" in f}


def _get_texture_slots(engine, mat_id=MATERIAL_ID):
    """Return {slot_name: texture_id} for the material's texture slots."""
    fields = _get_material_fields(engine, mat_id)
    return {
        name: val["texture"]
        for name, val in fields.items()
        if isinstance(val, dict) and "texture" in val
    }


class TestMaterialSaveRegression:

    def _check_all_objects_valid(self, engine):
        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

    def test_material_edit_does_not_break_rendering(self, engine):
        fields_before = _get_material_fields(engine)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        self._check_all_objects_valid(engine)

        engine.call("model.material.discard", {"id": MATERIAL_ID})
        self._check_all_objects_valid(engine)

        fields_after = _get_material_fields(engine)
        assert fields_after == fields_before, (
            f"edit+discard changed fields: {fields_before} -> {fields_after}"
        )

    def test_material_save_no_changes(self, engine):
        """Edit + save without modification — core pink bug scenario."""
        fields_before = _get_material_fields(engine)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

        self._check_all_objects_valid(engine)
        fields_after = _get_material_fields(engine)
        assert fields_after == fields_before, (
            f"no-op save changed fields: {fields_before} -> {fields_after}"
        )

    def test_material_setfield_then_save(self, engine):
        """Edit a field, save, verify the new value sticks and no pink bug."""
        fields_before = _get_material_fields(engine)
        original_band_count = fields_before["band_count"]
        new_band_count = original_band_count + 1.0

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call(
            "model.material.setField",
            {"id": MATERIAL_ID, "field": "band_count", "value": new_band_count},
        )
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

        self._check_all_objects_valid(engine)
        fields_after = _get_material_fields(engine)
        assert fields_after["band_count"] == new_band_count, (
            f"band_count not saved: expected {new_band_count}, got {fields_after['band_count']}"
        )

        # Restore original value
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call(
            "model.material.setField",
            {"id": MATERIAL_ID, "field": "band_count", "value": original_band_count},
        )
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

    def test_material_discard_reverts_field_changes(self, engine):
        """Edit + setField + discard must revert all fields to original."""
        fields_before = _get_material_fields(engine)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call(
            "model.material.setField",
            {"id": MATERIAL_ID, "field": "band_count", "value": 99},
        )
        engine.call("model.material.discard", {"id": MATERIAL_ID})

        self._check_all_objects_valid(engine)
        fields_after = _get_material_fields(engine)
        assert fields_after == fields_before, (
            f"discard didn't revert: {fields_before} -> {fields_after}"
        )

    def test_material_list_contains_test_material(self, engine):
        result = engine.call("model.material.list")
        mat_ids = [m["id"] for m in result.get("materials", [])]
        assert MATERIAL_ID in mat_ids, f"mt_toon not in material list: {mat_ids}"

    def test_texture_list_returns_textures(self, engine):
        result = engine.call("model.texture.list")
        tex_ids = [t["id"] for t in result.get("textures", [])]
        assert len(tex_ids) > 0, "no textures found"
        assert "txt_grey" in tex_ids, f"txt_grey not in texture list: {tex_ids}"

    def test_material_get_includes_texture_slots(self, engine):
        slots = _get_texture_slots(engine)
        assert "diffuse_txt" in slots, f"diffuse_txt slot missing: {slots}"
        assert slots["diffuse_txt"], "diffuse_txt has no texture assigned"

    def test_set_texture_then_save(self, engine):
        """Edit + setField(texture) + save — texture change persists, no pink bug."""
        original_slots = _get_texture_slots(engine)
        original_texture = original_slots["diffuse_txt"]
        new_texture = "txt_container"

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call(
            "model.material.setField",
            {"id": MATERIAL_ID, "field": "diffuse_txt", "value": new_texture},
        )
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

        self._check_all_objects_valid(engine)
        saved_slots = _get_texture_slots(engine)
        assert saved_slots["diffuse_txt"] == new_texture, (
            f"texture not saved: expected {new_texture}, got {saved_slots['diffuse_txt']}"
        )

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call(
            "model.material.setField",
            {"id": MATERIAL_ID, "field": "diffuse_txt", "value": original_texture},
        )
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

    def test_set_texture_discard_reverts(self, engine):
        """Edit + setField(texture) + discard — texture reverts to original."""
        original_slots = _get_texture_slots(engine)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call(
            "model.material.setField",
            {"id": MATERIAL_ID, "field": "diffuse_txt", "value": "txt_container"},
        )
        engine.call("model.material.discard", {"id": MATERIAL_ID})

        self._check_all_objects_valid(engine)
        reverted_slots = _get_texture_slots(engine)
        assert reverted_slots == original_slots, (
            f"discard didn't revert textures: {original_slots} -> {reverted_slots}"
        )

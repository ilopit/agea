"""Materials — edit/save/discard lifecycle, texture binding pipeline,
per-type property coverage, and runtime material assignment.

Material editing follows a session protocol:
  model.material.edit   → start edit session (locks material)
  model.material.setField → modify a property during session
  model.material.save   → commit changes to disk, rebuild render pipeline
  model.material.discard → revert all changes, release lock

The "pink bug" is the primary regression target: saving a material causes
objects to fall back to se_error (pink shader). Detectable via render layer:
  material.id == "se_error" or texture_indices all 0xFFFFFFFF.

Per-type property tests (pbr, solid_color, etc.) use save=False (edit+discard)
because saving non-toon materials triggers a render pipeline rebuild that may
fail for materials not currently rendered in the test scene. The edit+discard
path still exercises the property system end-to-end.
"""
import pytest
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions
from .property_helpers import (
    get_material_fields, get_texture_slots, assert_material_field_roundtrip,
    get_property,
)

MATERIAL_ID = "mt_toon"
ALT_MATERIAL_ID = "mt_solid_color"
TEXTURE_FIELD = "diffuse_txt"
RENDER_OBJECT = "hero_cube_mesh"
ALT_TEXTURE = "txt_container"


def _get_render_texture_indices(engine, obj_id=RENDER_OBJECT):
    ro = engine.call("render.object.data", {"id": obj_id})
    assertions.assert_not_pink_bug(ro, obj_id)
    return ro["material"]["texture_indices"]


# ---------------------------------------------------------------------------
# Edit/save lifecycle — pink bug regression
# ---------------------------------------------------------------------------

class TestMaterialEditSave:
    """Verify the material edit session lifecycle doesn't corrupt rendering.

    Every test checks that all render objects remain valid (no pink bug)
    after each operation. The core scenario: edit + save without changes
    must be a no-op for both model fields and render state.
    """

    def _check_all_objects_valid(self, engine):
        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

    def test_edit_discard_is_noop(self, engine):
        """edit + discard without changes preserves all fields and render state."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        self._check_all_objects_valid(engine)
        engine.call("model.material.discard", {"id": MATERIAL_ID})
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID) == fields_before

    def test_save_without_changes_is_noop(self, engine):
        """edit + save without modifications — core pink bug scenario."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID) == fields_before

    def test_setfield_save_persists(self, engine):
        """edit + setField + save — new value sticks, no pink bug."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        original = fields_before["band_count"]
        new_val = original + 1.0

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "band_count", "value": new_val,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID)["band_count"] == new_val

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "band_count", "value": original,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

    def test_discard_reverts_changes(self, engine):
        """edit + setField + discard reverts to original values."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "band_count", "value": 99,
        })
        engine.call("model.material.discard", {"id": MATERIAL_ID})
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID) == fields_before

    def test_material_list_contains_test_material(self, engine):
        """model.material.list includes mt_toon."""
        result = engine.call("model.material.list")
        mat_ids = [m["id"] for m in result.get("materials", [])]
        assert MATERIAL_ID in mat_ids

    def test_texture_list_nonempty(self, engine):
        """model.texture.list returns at least one texture including txt_grey."""
        result = engine.call("model.texture.list")
        tex_ids = [t["id"] for t in result.get("textures", [])]
        assert len(tex_ids) > 0
        assert "txt_grey" in tex_ids

    def test_material_has_texture_slots(self, engine):
        """mt_toon has a diffuse_txt slot with a texture assigned."""
        slots = get_texture_slots(engine, MATERIAL_ID)
        assert "diffuse_txt" in slots
        assert slots["diffuse_txt"]

    def test_set_texture_save_persists(self, engine):
        """edit + setField(texture) + save — texture change persists, no pink bug."""
        original_slots = get_texture_slots(engine, MATERIAL_ID)
        original = original_slots["diffuse_txt"]

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "diffuse_txt", "value": ALT_TEXTURE,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        self._check_all_objects_valid(engine)
        assert get_texture_slots(engine, MATERIAL_ID)["diffuse_txt"] == ALT_TEXTURE

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "diffuse_txt", "value": original,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()

    def test_set_texture_discard_reverts(self, engine):
        """edit + setField(texture) + discard reverts texture slot."""
        original_slots = get_texture_slots(engine, MATERIAL_ID)
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "diffuse_txt", "value": "txt_container",
        })
        engine.call("model.material.discard", {"id": MATERIAL_ID})
        self._check_all_objects_valid(engine)
        assert get_texture_slots(engine, MATERIAL_ID) == original_slots


# ---------------------------------------------------------------------------
# Texture swap — model + render two-layer verification
# ---------------------------------------------------------------------------

class TestTextureSwap:
    """Full model-to-render pipeline test for texture changes.

    Unlike TestMaterialEditSave (which checks model fields + pink bug),
    these tests verify that render.object.data.texture_indices actually
    change when we swap textures, and revert exactly on restore/discard.
    """

    def test_swap_updates_model_and_render(self, engine, slow):
        """T1 -> T2 -> T1: model texture slot and render texture_indices both update and restore."""
        original_slots = get_texture_slots(engine, MATERIAL_ID)
        original_texture = original_slots[TEXTURE_FIELD]
        assert original_texture != ALT_TEXTURE
        original_indices = _get_render_texture_indices(engine)
        assert not all(idx == assertions.UINT32_MAX for idx in original_indices)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": ALT_TEXTURE,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        slow("texture swapped to ALT")

        assert get_texture_slots(engine, MATERIAL_ID)[TEXTURE_FIELD] == ALT_TEXTURE
        swapped_indices = _get_render_texture_indices(engine)
        assert swapped_indices != original_indices, "Render texture_indices unchanged after swap"

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": original_texture,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        slow("texture restored")

        assert get_texture_slots(engine, MATERIAL_ID)[TEXTURE_FIELD] == original_texture
        assert _get_render_texture_indices(engine) == original_indices

    def test_discard_leaves_render_unchanged(self, engine, slow):
        """edit + setField + discard must not change render texture_indices."""
        original_indices = _get_render_texture_indices(engine)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": ALT_TEXTURE,
        })
        slow("texture set before discard")
        engine.call("model.material.discard", {"id": MATERIAL_ID})
        engine.wait_frame()
        slow("after discard")

        assert _get_render_texture_indices(engine) == original_indices

    def test_swap_does_not_corrupt_scene(self, engine, slow):
        """Texture swap on mt_toon must not pink-bug any render object."""
        original_texture = get_texture_slots(engine, MATERIAL_ID)[TEXTURE_FIELD]

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": ALT_TEXTURE,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        slow("texture swapped -- checking all objects")

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": TEXTURE_FIELD, "value": original_texture,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        slow("texture restored")


# ---------------------------------------------------------------------------
# Per-type material properties
#
# Each material type has its own reflected properties. These tests verify
# the property system handles them correctly via the edit session.
# Toon properties use save=True (known safe). Other types use save=False
# (edit+discard) because their render pipeline rebuild may crash when the
# material isn't actively rendered in the test scene.
# ---------------------------------------------------------------------------

def _find_material_of_type(engine, type_name):
    result = engine.call("model.material.list")
    for m in result.get("materials", []):
        if m.get("type") == type_name:
            return m["id"]
    return None


class TestToonMaterialProperties:
    """toon_material: band_count, specular_strength, shininess, diffuse_txt.
    Uses save=True — mt_toon is actively rendered so save is safe.
    """

    def test_band_count_roundtrip(self, engine):
        """Set band_count to 5.0, save, verify, restore original."""
        assert_material_field_roundtrip(engine, MATERIAL_ID, "band_count", 5.0)

    def test_specular_strength_roundtrip(self, engine):
        """Set specular_strength to 0.8, save, verify, restore original."""
        assert_material_field_roundtrip(engine, MATERIAL_ID, "specular_strength", 0.8)

    def test_shininess_roundtrip(self, engine):
        """Set shininess to 64.0, save, verify, restore original."""
        assert_material_field_roundtrip(engine, MATERIAL_ID, "shininess", 64.0)

    def test_diffuse_txt_roundtrip(self, engine):
        """Swap diffuse_txt to alt texture, save, verify, restore original."""
        original = get_texture_slots(engine, MATERIAL_ID)["diffuse_txt"]
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "diffuse_txt", "value": ALT_TEXTURE,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()
        assert get_texture_slots(engine, MATERIAL_ID)["diffuse_txt"] == ALT_TEXTURE
        engine.call("model.material.edit", {"id": MATERIAL_ID})
        engine.call("model.material.setField", {
            "id": MATERIAL_ID, "field": "diffuse_txt", "value": original,
        })
        engine.call("model.material.save", {"id": MATERIAL_ID})
        engine.wait_frame()


class TestPbrMaterialProperties:
    """pbr_material: ambient, diffuse, specular (vec3), diffuse_txt, specular_txt.
    Uses save=False — pbr may not be rendered in test scene.
    """

    def _get_pbr(self, engine):
        mat_id = _find_material_of_type(engine, "pbr_material")
        if mat_id is None:
            pytest.skip("No pbr_material instance found in loaded packages")
        return mat_id

    def test_ambient_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_pbr(engine), "ambient", [0.2, 0.2, 0.2], save=False)

    def test_diffuse_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_pbr(engine), "diffuse", [0.8, 0.1, 0.1], save=False)

    def test_specular_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_pbr(engine), "specular", [1.0, 1.0, 1.0], save=False)

    def test_diffuse_txt_setfield(self, engine):
        """setField on diffuse_txt accepts a texture ID without error."""
        mat_id = self._get_pbr(engine)
        if "diffuse_txt" not in get_texture_slots(engine, mat_id):
            pytest.skip("pbr_material has no diffuse_txt slot")
        engine.call("model.material.edit", {"id": mat_id})
        engine.call("model.material.setField", {
            "id": mat_id, "field": "diffuse_txt", "value": ALT_TEXTURE,
        })
        engine.call("model.material.discard", {"id": mat_id})

    def test_specular_txt_setfield(self, engine):
        """setField on specular_txt accepts a texture ID without error."""
        mat_id = self._get_pbr(engine)
        if "specular_txt" not in get_texture_slots(engine, mat_id):
            pytest.skip("pbr_material has no specular_txt slot")
        engine.call("model.material.edit", {"id": mat_id})
        engine.call("model.material.setField", {
            "id": mat_id, "field": "specular_txt", "value": ALT_TEXTURE,
        })
        engine.call("model.material.discard", {"id": mat_id})


class TestSolidColorMaterialProperties:
    """solid_color_material: ambient, diffuse, specular (vec3), shininess (float).
    Uses save=False.
    """

    def _get_solid(self, engine):
        mat_id = _find_material_of_type(engine, "solid_color_material")
        if mat_id is None:
            pytest.skip("No solid_color_material instance found")
        return mat_id

    def test_ambient_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_solid(engine), "ambient", [0.3, 0.3, 0.3], save=False)

    def test_diffuse_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_solid(engine), "diffuse", [0.7, 0.2, 0.2], save=False)

    def test_specular_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_solid(engine), "specular", [1.0, 1.0, 1.0], save=False)

    def test_shininess_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_solid(engine), "shininess", 32.0, save=False)


class TestSolidColorAlphaMaterialProperties:
    """solid_color_alpha_material: opacity (float). Uses save=False."""

    def _get_alpha(self, engine):
        mat_id = _find_material_of_type(engine, "solid_color_alpha_material")
        if mat_id is None:
            pytest.skip("No solid_color_alpha_material instance found")
        return mat_id

    def test_opacity_roundtrip(self, engine):
        assert_material_field_roundtrip(engine, self._get_alpha(engine), "opacity", 0.5, save=False)


class TestSimpleTextureMaterialProperties:
    """simple_texture_material: simple_texture (texture_slot). Uses save=False."""

    def _get_simple(self, engine):
        mat_id = _find_material_of_type(engine, "simple_texture_material")
        if mat_id is None:
            pytest.skip("No simple_texture_material instance found")
        return mat_id

    def test_simple_texture_setfield(self, engine):
        """setField on simple_texture accepts a texture ID without error."""
        mat_id = self._get_simple(engine)
        engine.call("model.material.edit", {"id": mat_id})
        engine.call("model.material.setField", {
            "id": mat_id, "field": "simple_texture", "value": ALT_TEXTURE,
        })
        engine.call("model.material.discard", {"id": mat_id})


# ---------------------------------------------------------------------------
# Material assign — swap material reference on a mesh_component via RPC
# ---------------------------------------------------------------------------

class TestMaterialAssign:
    """Verify model.material.assign swaps the material on an existing
    mesh_component and the render cache reflects the new material ID.
    """

    def test_assign_and_restore(self, engine, slow):
        """Assign mt_solid_color to hero_cube, verify model+render, restore mt_toon."""
        engine.call("model.material.assign", {
            "material_id": ALT_MATERIAL_ID, "owner_id": "hero_cube_mesh",
        })
        engine.wait_frame()
        slow("material -> solid_color")

        model_mat = get_property(engine, "hero_cube_mesh", "material")
        assert model_mat == ALT_MATERIAL_ID, f"Model: expected {ALT_MATERIAL_ID}, got {model_mat}"

        ro = engine.call("render.object.data", {"id": "hero_cube_mesh"})
        assert ro["material"]["id"] == ALT_MATERIAL_ID

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro_check = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro_check, comp_id)

        engine.call("model.material.assign", {
            "material_id": MATERIAL_ID, "owner_id": "hero_cube_mesh",
        })
        engine.wait_frame()
        slow("material -> toon")

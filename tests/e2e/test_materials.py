"""Materials — edit/save/discard lifecycle, texture binding pipeline,
per-type property coverage, and runtime material assignment.

Material editing follows a session protocol:
  editor.material.edit   → start edit session, returns edit_id
  model.object.property.set(edit_id, field, value) → modify a property
  editor.material.save   → commit changes to disk, rebuild render pipeline
  editor.material.discard → revert all changes, release session

The "pink bug" is the primary regression target: saving a material causes
objects to fall back to se_error (pink shader). Detectable via render layer:
  material.id == "se_error" or texture_indices all 0xFFFFFFFF.

Per-type property tests assign the material under test to hero_cube_mesh so
the full model → render pipeline is exercised. Each test edits a property,
saves, then verifies both model fields and render-layer state (no pink bug).
"""
import pytest
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions
from .property_helpers import (
    get_material_fields, get_texture_slots, assert_material_field_roundtrip,
    get_property, set_property,
    material_begin_edit, material_set_field, material_save, material_discard,
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


def _find_material_of_type(engine, type_name):
    result = engine.call("model.list", {"source": "all", "kind": "material"})
    for m in result.get("items", []):
        if m.get("type_name") == type_name:
            return m["id"]
    return None


def _check_all_render_objects(engine):
    for comp_id in EXPECTED_RENDER_OBJECTS:
        ro = engine.call("render.object.data", {"id": comp_id})
        assertions.assert_not_pink_bug(ro, comp_id)


def _edit_save_verify(engine, mat_id, field, value, slow=None):
    """Edit a material field, save, verify model + render, restore original."""
    original = get_material_fields(engine, mat_id).get(field)

    edit_id = material_begin_edit(engine, mat_id)
    material_set_field(engine, edit_id, field, value)
    material_save(engine, mat_id)

    fields = get_material_fields(engine, mat_id)
    if isinstance(value, (list, tuple)):
        for i, (a, e) in enumerate(zip(fields[field], value)):
            assert abs(a - e) < 0.01, f"{field}[{i}]: got {a}, expected {e}"
    elif isinstance(value, float):
        assert abs(fields[field] - value) < 0.01, f"{field}: got {fields[field]}, expected {value}"
    else:
        assert fields[field] == value, f"{field}: got {fields[field]}, expected {value}"

    _check_all_render_objects(engine)
    if slow:
        slow(f"{field} = {value}")

    edit_id = material_begin_edit(engine, mat_id)
    material_set_field(engine, edit_id, field, original)
    material_save(engine, mat_id)
    if slow:
        slow(f"{field} restored")


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
        material_begin_edit(engine, MATERIAL_ID)
        self._check_all_objects_valid(engine)
        material_discard(engine, MATERIAL_ID)
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID) == fields_before

    def test_save_without_changes_is_noop(self, engine):
        """edit + save without modifications — core pink bug scenario."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        material_begin_edit(engine, MATERIAL_ID)
        material_save(engine, MATERIAL_ID)
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID) == fields_before

    def test_setfield_save_persists(self, engine):
        """edit + setField + save — new value sticks, no pink bug."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        original = fields_before["band_count"]
        new_val = original + 1.0

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "band_count", new_val)
        material_save(engine, MATERIAL_ID)
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID)["band_count"] == new_val

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "band_count", original)
        material_save(engine, MATERIAL_ID)

    def test_discard_reverts_changes(self, engine):
        """edit + setField + discard reverts to original values."""
        fields_before = get_material_fields(engine, MATERIAL_ID)
        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "band_count", 99)
        material_discard(engine, MATERIAL_ID)
        self._check_all_objects_valid(engine)
        assert get_material_fields(engine, MATERIAL_ID) == fields_before

    def test_material_list_contains_test_material(self, engine):
        """model.list with kind=material includes mt_toon."""
        result = engine.call("model.list", {"source": "all", "kind": "material"})
        mat_ids = [m["id"] for m in result.get("items", [])]
        assert MATERIAL_ID in mat_ids

    def test_texture_list_nonempty(self, engine):
        """model.list with kind=texture returns at least one texture including txt_grey."""
        result = engine.call("model.list", {"source": "all", "kind": "texture"})
        tex_ids = [t["id"] for t in result.get("items", [])]
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

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "diffuse_txt", ALT_TEXTURE)
        material_save(engine, MATERIAL_ID)
        self._check_all_objects_valid(engine)
        assert get_texture_slots(engine, MATERIAL_ID)["diffuse_txt"] == ALT_TEXTURE

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "diffuse_txt", original)
        material_save(engine, MATERIAL_ID)

    def test_set_texture_discard_reverts(self, engine):
        """edit + setField(texture) + discard reverts texture slot."""
        original_slots = get_texture_slots(engine, MATERIAL_ID)
        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "diffuse_txt", "txt_container")
        material_discard(engine, MATERIAL_ID)
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

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, TEXTURE_FIELD, ALT_TEXTURE)
        material_save(engine, MATERIAL_ID)

        assert get_texture_slots(engine, MATERIAL_ID)[TEXTURE_FIELD] == ALT_TEXTURE
        swapped_indices = _get_render_texture_indices(engine)
        assert swapped_indices != original_indices, "Render texture_indices unchanged after swap"
        slow("texture swapped to ALT")

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, TEXTURE_FIELD, original_texture)
        material_save(engine, MATERIAL_ID)

        assert get_texture_slots(engine, MATERIAL_ID)[TEXTURE_FIELD] == original_texture
        assert _get_render_texture_indices(engine) == original_indices
        slow("texture restored")

    def test_discard_leaves_render_unchanged(self, engine, slow):
        """edit + setField + discard must not change render texture_indices."""
        original_indices = _get_render_texture_indices(engine)

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, TEXTURE_FIELD, ALT_TEXTURE)
        slow("texture set before discard")
        material_discard(engine, MATERIAL_ID)
        engine.wait_frame()
        slow("after discard")

        assert _get_render_texture_indices(engine) == original_indices

    def test_swap_does_not_corrupt_scene(self, engine, slow):
        """Texture swap on mt_toon must not pink-bug any render object."""
        original_texture = get_texture_slots(engine, MATERIAL_ID)[TEXTURE_FIELD]

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, TEXTURE_FIELD, ALT_TEXTURE)
        material_save(engine, MATERIAL_ID)

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)
        slow("texture swapped -- all objects valid")

        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, TEXTURE_FIELD, original_texture)
        material_save(engine, MATERIAL_ID)
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

class TestToonMaterialProperties:
    """toon_material: band_count, specular_strength, shininess, diffuse_txt.
    mt_toon is already on hero_cube_mesh — no assign needed.
    """

    def test_band_count_roundtrip(self, engine, slow):
        _edit_save_verify(engine, MATERIAL_ID, "band_count", 5.0, slow)

    def test_specular_strength_roundtrip(self, engine, slow):
        _edit_save_verify(engine, MATERIAL_ID, "specular_strength", 0.8, slow)

    def test_shininess_roundtrip(self, engine, slow):
        _edit_save_verify(engine, MATERIAL_ID, "shininess", 64.0, slow)

    def test_diffuse_txt_roundtrip(self, engine, slow):
        original = get_texture_slots(engine, MATERIAL_ID)["diffuse_txt"]
        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "diffuse_txt", ALT_TEXTURE)
        material_save(engine, MATERIAL_ID)
        assert get_texture_slots(engine, MATERIAL_ID)["diffuse_txt"] == ALT_TEXTURE
        _check_all_render_objects(engine)
        slow("diffuse_txt -> alt")
        edit_id = material_begin_edit(engine, MATERIAL_ID)
        material_set_field(engine, edit_id, "diffuse_txt", original)
        material_save(engine, MATERIAL_ID)
        slow("diffuse_txt restored")


class TestSolidColorMaterialProperties:
    """solid_color_material: ambient, diffuse, specular (vec3), shininess (float).
    Assigned to hero_cube_mesh — solid_color is known safe on the test mesh.
    """

    @pytest.fixture(autouse=True)
    def _assign_material(self, engine, slow):
        self.mat_id = _find_material_of_type(engine, "solid_color_material")
        if self.mat_id is None:
            pytest.skip("No solid_color_material instance found")
        set_property(engine, RENDER_OBJECT, "material", self.mat_id)
        slow(f"assigned {self.mat_id} to {RENDER_OBJECT}")
        _check_all_render_objects(engine)
        yield
        set_property(engine, RENDER_OBJECT, "material", MATERIAL_ID)
        slow(f"restored {MATERIAL_ID}")

    def test_ambient_roundtrip(self, engine, slow):
        _edit_save_verify(engine, self.mat_id, "ambient", [0.3, 0.3, 0.3], slow)

    def test_diffuse_roundtrip(self, engine, slow):
        _edit_save_verify(engine, self.mat_id, "diffuse", [0.7, 0.2, 0.2], slow)

    def test_specular_roundtrip(self, engine, slow):
        _edit_save_verify(engine, self.mat_id, "specular", [1.0, 1.0, 1.0], slow)

    def test_shininess_roundtrip(self, engine, slow):
        _edit_save_verify(engine, self.mat_id, "shininess", 32.0, slow)


class _EditDiscardBase:
    """Base for per-type tests that can't safely assign to the test mesh.

    Uses edit+discard to exercise the property system without triggering a
    render pipeline rebuild. Still verifies no corruption after discard.
    """
    MAT_TYPE = None

    @pytest.fixture(autouse=True)
    def _find_material(self, engine):
        self.mat_id = _find_material_of_type(engine, self.MAT_TYPE)
        if self.mat_id is None:
            pytest.skip(f"No {self.MAT_TYPE} instance found in loaded packages")

    def _edit_discard_verify(self, engine, field, value, slow):
        original = get_material_fields(engine, self.mat_id).get(field)
        edit_id = material_begin_edit(engine, self.mat_id)
        material_set_field(engine, edit_id, field, value)
        slow(f"{field} = {value} (on edit instance)")

        edit_fields = get_material_fields(engine, edit_id)
        if isinstance(value, (list, tuple)):
            for i, (a, e) in enumerate(zip(edit_fields[field], value)):
                assert abs(a - e) < 0.01, f"{field}[{i}]: got {a}, expected {e}"
        elif isinstance(value, float):
            assert abs(edit_fields[field] - value) < 0.01, f"{field}: got {edit_fields[field]}, expected {value}"
        else:
            assert edit_fields[field] == value, f"{field}: got {edit_fields[field]}, expected {value}"

        material_discard(engine, self.mat_id)
        slow(f"{field} discarded")
        _check_all_render_objects(engine)
        assert get_material_fields(engine, self.mat_id).get(field) == original


class TestPbrMaterialProperties(_EditDiscardBase):
    """pbr_material: ambient, diffuse, specular, texture slots.
    Edit+discard — assigning pbr to the test mesh crashes the render pipeline.
    """
    MAT_TYPE = "pbr_material"

    def test_ambient_roundtrip(self, engine, slow):
        self._edit_discard_verify(engine, "ambient", [0.2, 0.2, 0.2], slow)

    def test_diffuse_roundtrip(self, engine, slow):
        self._edit_discard_verify(engine, "diffuse", [0.8, 0.1, 0.1], slow)

    def test_specular_roundtrip(self, engine, slow):
        self._edit_discard_verify(engine, "specular", [1.0, 1.0, 1.0], slow)

    def test_diffuse_txt_setfield(self, engine, slow):
        if "diffuse_txt" not in get_texture_slots(engine, self.mat_id):
            pytest.skip("pbr_material has no diffuse_txt slot")
        edit_id = material_begin_edit(engine, self.mat_id)
        material_set_field(engine, edit_id, "diffuse_txt", ALT_TEXTURE)
        slow("diffuse_txt -> alt (on edit instance)")
        material_discard(engine, self.mat_id)

    def test_specular_txt_setfield(self, engine, slow):
        if "specular_txt" not in get_texture_slots(engine, self.mat_id):
            pytest.skip("pbr_material has no specular_txt slot")
        edit_id = material_begin_edit(engine, self.mat_id)
        material_set_field(engine, edit_id, "specular_txt", ALT_TEXTURE)
        slow("specular_txt -> alt (on edit instance)")
        material_discard(engine, self.mat_id)


class TestSolidColorAlphaMaterialProperties(_EditDiscardBase):
    """solid_color_alpha_material: opacity (float).
    Edit+discard — may not be safe to assign to test mesh.
    """
    MAT_TYPE = "solid_color_alpha_material"

    def test_opacity_roundtrip(self, engine, slow):
        self._edit_discard_verify(engine, "opacity", 0.5, slow)


class TestSimpleTextureMaterialProperties(_EditDiscardBase):
    """simple_texture_material: simple_texture (texture_slot).
    Edit+discard — may not be safe to assign to test mesh.
    """
    MAT_TYPE = "simple_texture_material"

    def test_simple_texture_setfield(self, engine, slow):
        if "simple_texture" not in get_texture_slots(engine, self.mat_id):
            pytest.skip("simple_texture_material has no simple_texture slot")
        edit_id = material_begin_edit(engine, self.mat_id)
        material_set_field(engine, edit_id, "simple_texture", ALT_TEXTURE)
        slow("simple_texture -> alt (on edit instance)")
        material_discard(engine, self.mat_id)


# ---------------------------------------------------------------------------
# Material assign — swap material reference on a mesh_component via property.set
# ---------------------------------------------------------------------------

class TestMaterialAssign:
    """Verify property.set on mesh_component's 'material' swaps the material
    and the render cache reflects the new material ID.
    """

    def test_assign_and_restore(self, engine, slow):
        """Assign mt_solid_color to hero_cube, verify model+render, restore mt_toon."""
        set_property(engine, "hero_cube_mesh", "material", ALT_MATERIAL_ID)
        slow("material -> solid_color")

        model_mat = get_property(engine, "hero_cube_mesh", "material")
        assert model_mat == ALT_MATERIAL_ID, f"Model: expected {ALT_MATERIAL_ID}, got {model_mat}"

        ro = engine.call("render.object.data", {"id": "hero_cube_mesh"})
        assert ro["material"]["id"] == ALT_MATERIAL_ID

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro_check = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro_check, comp_id)

        set_property(engine, "hero_cube_mesh", "material", MATERIAL_ID)
        slow("material -> toon")

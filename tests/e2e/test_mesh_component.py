"""Mesh component — mesh/material property round-trips and render propagation.

Covers the two reflected properties on mesh_component:
- m_mesh (mesh*, invalidates=render) — swap mesh asset, verify render cache updates
- m_material (material*, invalidates=render) — swap material, verify render cache updates

Both properties trigger mark_render_dirty(), so we verify the render layer
reflects the change after wait_frame().
"""
import pytest
from base import mesh_component
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

HERO_MESH = "hero_cube_mesh"
MESH_ID = "cube_mesh"
ALT_MESH = "plane_mesh"
MATERIAL_ID = "mt_toon"
ALT_MATERIAL_ID = "mt_solid_color"


# ---------------------------------------------------------------------------
# Mesh property (m_mesh)
# ---------------------------------------------------------------------------

class TestMeshProperty:
    """Swap the mesh asset reference on hero_cube's mesh_component.
    Verify the render cache picks up the new mesh ID and no pink bug occurs.
    """

    def test_set_mesh_updates_render(self, engine, slow):
        """set_mesh(plane) → render shows plane_mesh; restore → cube_mesh."""
        mc = mesh_component(engine, HERO_MESH)
        ro = mc.get_render_data()
        original = ro["mesh"]["id"]
        assert original == MESH_ID

        mc.set_mesh(ALT_MESH)
        engine.wait_frame()
        slow("mesh -> plane")

        ro = mc.get_render_data()
        assert ro["mesh"]["id"] == ALT_MESH
        assertions.assert_not_pink_bug(ro, HERO_MESH)

        mc.set_mesh(original)
        engine.wait_frame()
        slow("mesh -> cube")
        assert mc.get_render_data()["mesh"]["id"] == original

    def test_set_mesh_does_not_corrupt_scene(self, engine, slow):
        """Swapping hero_cube's mesh doesn't pink-bug other objects."""
        mc = mesh_component(engine, HERO_MESH)
        mc.set_mesh(ALT_MESH)
        engine.wait_frame()
        slow("mesh -> plane")

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        mc.set_mesh(MESH_ID)
        engine.wait_frame()
        slow("mesh -> cube")

    def test_get_mesh_returns_current(self, engine):
        """get_mesh returns the asset ID currently assigned."""
        mc = mesh_component(engine, HERO_MESH)
        assert mc.get_mesh() == MESH_ID


# ---------------------------------------------------------------------------
# Material property (m_material)
# ---------------------------------------------------------------------------

class TestMaterialProperty:
    """Swap the material asset reference on hero_cube's mesh_component.
    Verify the render cache picks up the new material ID.

    NOTE: model.material.assign crashes the engine (known bug), so we test
    via the property proxy (model.object.property.set) instead.
    """

    def test_set_material_updates_render(self, engine, slow):
        """set_material(alt) → render shows alt material; restore → mt_toon."""
        alt_mat = ALT_MATERIAL_ID

        mc = mesh_component(engine, HERO_MESH)
        original = mc.get_material()

        mc.set_material(alt_mat)
        engine.wait_frame()
        slow(f"material -> {alt_mat}")

        ro = mc.get_render_data()
        assert ro["material"]["id"] == alt_mat
        assertions.assert_not_pink_bug(ro, HERO_MESH)

        mc.set_material(original)
        engine.wait_frame()
        slow("material -> mt_toon")

    def test_get_material_returns_current(self, engine):
        """get_material returns mt_toon on hero_cube."""
        mc = mesh_component(engine, HERO_MESH)
        assert mc.get_material() == MATERIAL_ID

"""Terrain component: heightfield mesh generation + splatmap material, model + render layers."""
from . import assertions
from .property_helpers import get_property

TERRAIN_LEVEL = "terrain_sandbox"
TERRAIN_COMPONENT = "terrain_comp"
TERRAIN_MATERIAL = "mt_terrain"
TERRAIN_MESH = "terrain_comp::terrain_mesh"

# 100x100 noise terrain, height_scale 18 -> sphere radius ~= sqrt(50^2+50^2+9^2)
EXPECTED_BOUNDING_RADIUS = 71.0
BOUNDING_RADIUS_TOL = 5.0

UINT32_MAX = 0xFFFFFFFF


def _load_terrain(engine):
    engine.load_level_and_wait(TERRAIN_LEVEL, settle_time=1.0, timeout=15.0)
    engine.wait_frame()


def _render_object(engine, obj_id):
    objs = engine.call("render.object.list")
    return assertions.assert_render_object_exists(objs, obj_id)


class TestTerrain:

    def test_terrain_model_state(self, engine):
        """Model layer: the component deserialized the authored terrain parameters.

        (The material pointer binding is verified at the render layer below —
        model.object.property.get reports pointer fields with an empty value.)
        """
        _load_terrain(engine)

        # noise source (1) selected via the integer source_mode property
        assert int(get_property(engine, TERRAIN_COMPONENT, "source_mode")) == 1
        assert int(get_property(engine, TERRAIN_COMPONENT, "resolution")) == 128
        assert int(get_property(engine, TERRAIN_COMPONENT, "octaves")) == 5

    def test_terrain_renders_with_splat_material(self, engine):
        """Render layer: a generated mesh + the 5-texture splat material reach the GPU."""
        _load_terrain(engine)

        ro = engine.call("render.object.data", {"id": TERRAIN_COMPONENT})

        # Generated terrain mesh is bound (derived id, not an asset).
        assertions.assert_mesh_matches(ro, TERRAIN_MESH, TERRAIN_COMPONENT)
        # Terrain splat material bound, not the pink error fallback.
        assertions.assert_material_id_matches(ro, TERRAIN_MATERIAL, TERRAIN_COMPONENT)
        assertions.assert_not_pink_bug(ro, TERRAIN_COMPONENT)

        # Splatmap (slot 0) + 4 layer albedos (slots 1-4) must be bound; the
        # widened slot table leaves the rest unbound.
        tex = ro["material"]["texture_indices"]
        bound = [i for i in tex if i != UINT32_MAX]
        assert len(bound) == 5, f"expected 5 bound terrain textures, got {len(bound)}: {tex}"

    def test_terrain_bounding_volume(self, engine):
        """The generated heightfield produces a plausible world-space bounding sphere."""
        _load_terrain(engine)

        obj = _render_object(engine, TERRAIN_COMPONENT)
        assert obj.get("renderable", False), "terrain object is not renderable"
        radius = obj["bounding_radius"]
        assert abs(radius - EXPECTED_BOUNDING_RADIUS) < BOUNDING_RADIUS_TOL, (
            f"terrain bounding radius {radius} not near expected {EXPECTED_BOUNDING_RADIUS}"
        )

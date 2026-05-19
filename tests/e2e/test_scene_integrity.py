"""Scene integrity after loading simple_test level."""
from .conftest import EXPECTED_SCENE_OBJECTS, EXPECTED_RENDER_OBJECTS
from . import assertions


class TestSceneIntegrity:

    def test_all_expected_objects_present(self, engine, load_test_level):
        obj_ids = {child["id"] for child in load_test_level.get("children", [])}
        assert EXPECTED_SCENE_OBJECTS == obj_ids

    def test_object_count_exact(self, engine, load_test_level):
        assert len(load_test_level["children"]) == len(EXPECTED_SCENE_OBJECTS)

    def test_render_objects_have_correct_meshes(self, engine):
        for comp_id, expected in EXPECTED_RENDER_OBJECTS.items():
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_mesh_matches(ro, expected["mesh"], comp_id)

    def test_render_objects_have_correct_materials(self, engine):
        for comp_id, expected in EXPECTED_RENDER_OBJECTS.items():
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_material_id_matches(ro, expected["material"], comp_id)

    def test_no_pink_bug_on_clean_load(self, engine):
        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

    def test_render_stats_object_count(self, engine):
        stats = engine.call("render.stats")
        assert stats["object_count"] >= 2

    def test_directional_light_present(self, engine):
        lights = engine.call("render.lights.data")
        dir_lights = lights.get("directional", [])
        light_ids = [dl["id"] for dl in dir_lights]
        assert "sun_lc" in light_ids, f"Sun light not found. Lights: {light_ids}"

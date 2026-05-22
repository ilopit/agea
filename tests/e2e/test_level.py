"""Level lifecycle — CRUD, integrity checks, save/reload round-trips."""
from .conftest import EXPECTED_SCENE_OBJECTS, EXPECTED_RENDER_OBJECTS
from . import assertions

NEW_OBJECT = "test_crud_object"


def _get_render_object_ids(engine):
    objs = engine.call("render.object.list")
    return {o["id"] for o in objs.get("objects", [])}


# ---------------------------------------------------------------------------
# CRUD
# ---------------------------------------------------------------------------

class TestLevelCRUD:

    def test_create_appears_in_scene(self, engine):
        render_ids_before = _get_render_object_ids(engine)

        engine.call("model.scene.create", {"name": NEW_OBJECT})

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert NEW_OBJECT in obj_ids

        render_ids_after = _get_render_object_ids(engine)
        assert render_ids_before == render_ids_after, (
            "Empty object creation changed render objects"
        )

    def test_create_increases_count(self, engine):
        root_before = engine.call("model.scene.getRoot")
        count_before = len(root_before["children"])

        engine.call("model.scene.create", {"name": NEW_OBJECT})

        root_after = engine.call("model.scene.getRoot")
        assert len(root_after["children"]) == count_before + 1

    def test_delete_removes_from_scene(self, engine):
        engine.call("model.scene.create", {"name": NEW_OBJECT})
        engine.call("model.scene.delete", {"id": NEW_OBJECT})

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert NEW_OBJECT not in obj_ids

    def test_delete_restores_count(self, engine):
        engine.call("model.scene.create", {"name": NEW_OBJECT})
        engine.call("model.scene.delete", {"id": NEW_OBJECT})

        root = engine.call("model.scene.getRoot")
        assert len(root["children"]) == len(EXPECTED_SCENE_OBJECTS)

    def test_duplicate_appears_in_model_and_render(self, engine):
        render_ids_before = _get_render_object_ids(engine)

        result = engine.call("model.scene.duplicate", {"id": "hero_cube"})
        clone_id = result["id"]
        engine.wait_frame()

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert clone_id in obj_ids

        render_ids_after = _get_render_object_ids(engine)
        new_render_ids = render_ids_after - render_ids_before
        assert len(new_render_ids) > 0, (
            f"Duplicate of hero_cube produced no new render objects"
        )

        engine.call("model.scene.delete", {"id": clone_id})

    def test_delete_removes_from_model_no_corruption(self, engine):
        result = engine.call("model.scene.duplicate", {"id": "hero_cube"})
        clone_id = result["id"]
        engine.wait_frame()

        engine.call("model.scene.delete", {"id": clone_id})
        engine.wait_frame()

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert clone_id not in obj_ids

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

    def test_crud_does_not_corrupt_materials(self, engine):
        engine.call("model.scene.create", {"name": NEW_OBJECT})
        engine.call("model.scene.delete", {"id": NEW_OBJECT})

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)


# ---------------------------------------------------------------------------
# Integrity
# ---------------------------------------------------------------------------

class TestLevelIntegrity:

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


# ---------------------------------------------------------------------------
# Round-trip
# ---------------------------------------------------------------------------

class TestLevelRoundtrip:

    def test_reload_preserves_model_and_render(self, engine):
        root_before = engine.call("model.scene.getRoot")
        ids_before = {c["id"] for c in root_before["children"]}
        render_ids_before = _get_render_object_ids(engine)

        engine.load_level_and_wait("simple_test")

        root_after = engine.call("model.scene.getRoot")
        ids_after = {c["id"] for c in root_after["children"]}
        assert ids_before == ids_after

        render_ids_after = _get_render_object_ids(engine)
        assert render_ids_before == render_ids_after, (
            f"Render objects changed after reload: "
            f"lost={render_ids_before - render_ids_after}, "
            f"gained={render_ids_after - render_ids_before}"
        )

    def test_save_reload_preserves_model_and_render(self, engine):
        root_before = engine.call("model.scene.getRoot")
        ids_before = {c["id"] for c in root_before["children"]}

        engine.call("model.level.save")
        engine.load_level_and_wait("simple_test")

        root_after = engine.call("model.scene.getRoot")
        ids_after = {c["id"] for c in root_after["children"]}
        assert ids_before == ids_after

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

    def test_unsaved_object_does_not_persist(self, engine):
        engine.call("model.scene.create", {"name": "ephemeral_obj"})
        engine.wait_frame()

        engine.load_level_and_wait("simple_test")

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert "ephemeral_obj" not in obj_ids

"""Scene CRUD — create, duplicate, delete objects."""
from .conftest import EXPECTED_SCENE_OBJECTS, EXPECTED_RENDER_OBJECTS
from . import assertions

NEW_OBJECT = "test_crud_object"


def _get_render_object_ids(engine):
    objs = engine.call("render.object.list")
    return {o["id"] for o in objs.get("objects", [])}


class TestSceneCRUD:

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

"""Scene CRUD — create, duplicate, delete objects."""
import pytest

from .conftest import EXPECTED_SCENE_OBJECTS
from . import assertions

NEW_OBJECT = "test_crud_object"


class TestSceneCRUD:

    def test_create_appears_in_scene(self, engine):
        engine.call("model.scene.create", {"name": NEW_OBJECT})
        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert NEW_OBJECT in obj_ids

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

    def test_duplicate_creates_clone(self, engine):
        result = engine.call("model.scene.duplicate", {"id": "hero_cube"})
        clone_id = result["id"]

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert clone_id in obj_ids

    def test_crud_does_not_corrupt_materials(self, engine):
        engine.call("model.scene.create", {"name": NEW_OBJECT})
        engine.call("model.scene.delete", {"id": NEW_OBJECT})

        for comp_id in ("hero_cube_mesh", "ground_mesh"):
            ro = engine.call("render.state.object", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

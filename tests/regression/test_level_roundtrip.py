"""Level save/reload round-trip — verify level structure survives save/reload."""
from . import assertions


class TestLevelRoundtrip:

    def test_reload_preserves_scene_objects(self, engine):
        """All original objects survive a model.level.load cycle."""
        root_before = engine.call("model.scene.getRoot")
        ids_before = {c["id"] for c in root_before["children"]}

        engine.load_level_and_wait("simple_test")

        root_after = engine.call("model.scene.getRoot")
        ids_after = {c["id"] for c in root_after["children"]}
        assert ids_before == ids_after

    def test_materials_valid_after_save_reload(self, engine):
        engine.call("model.level.save")
        engine.load_level_and_wait("simple_test")

        for comp_id in ("hero_cube_mesh", "ground_mesh"):
            ro = engine.call("render.state.object", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

    def test_unsaved_object_does_not_persist(self, engine):
        engine.call("model.scene.create", {"name": "ephemeral_obj"})

        engine.load_level_and_wait("simple_test")

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert "ephemeral_obj" not in obj_ids

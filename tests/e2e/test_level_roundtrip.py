"""Level save/reload round-trip — verify level structure survives save/reload."""
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions


def _get_render_object_ids(engine):
    objs = engine.call("render.object.list")
    return {o["id"] for o in objs.get("objects", [])}


class TestLevelRoundtrip:

    def test_reload_preserves_model_and_render(self, engine):
        """All original objects survive a model.level.load cycle in both layers."""
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
        """Save + reload: both scene graph and render state survive."""
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

        render_ids_with_ephemeral = _get_render_object_ids(engine)

        engine.load_level_and_wait("simple_test")

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert "ephemeral_obj" not in obj_ids

        render_ids_after = _get_render_object_ids(engine)
        assert render_ids_after == render_ids_after - {"ephemeral_obj"}, (
            "Ephemeral object still present in render state after reload"
        )

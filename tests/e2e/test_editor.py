"""Editor operations — selection, engine mode, editor camera.

Covers editor-only RPC endpoints that don't map directly to model/render
object properties:
- model.selection.set/get — editor selection state (model-layer only, no render counterpart)
- engine.getMode/setMode — edit↔play mode transition, verifying scene survives
- editor.camera.set + render.camera.data — editor camera → render camera pipeline
"""
import pytest
from .conftest import EXPECTED_SCENE_OBJECTS, EXPECTED_RENDER_OBJECTS, TEST_CAMERA
from . import assertions


# ---------------------------------------------------------------------------
# Selection
# ---------------------------------------------------------------------------

class TestSelection:
    """Verify editor selection round-trips through model.selection set/get.

    Selection is a pure editor concept — no render-layer counterpart to check.
    We verify that set→get returns the same ID, and that changing selection
    overwrites the previous value.
    """

    def test_set_and_get_roundtrip(self, engine):
        """set(hero_cube) → get returns hero_cube."""
        engine.call("model.selection.set", {"id": "hero_cube"})
        result = engine.call("model.selection.get")
        assert result.get("id") == "hero_cube"

    def test_selection_overwrites_previous(self, engine):
        """Two consecutive sets — get returns the latest, not the first."""
        engine.call("model.selection.set", {"id": "hero_cube"})
        engine.call("model.selection.set", {"id": "ground"})
        result = engine.call("model.selection.get")
        assert result.get("id") == "ground"


# ---------------------------------------------------------------------------
# Engine mode
# ---------------------------------------------------------------------------

class TestEngineMode:
    """Verify edit↔play mode transitions and that scene state survives.

    Switching to play mode and back is a common editor operation. The scene
    graph and render cache must not lose objects during the transition.
    """

    def test_default_mode_is_edit(self, engine):
        """Engine starts in edit mode."""
        result = engine.call("engine.getMode")
        assert result.get("mode") == "edit"

    @pytest.mark.skip(reason="play→edit state restore not implemented yet")
    def test_play_edit_roundtrip_restores_transforms(self, engine):
        """Move an object in play mode, switch back — edit-mode position restored."""
        from root import game_object_component
        comp = game_object_component(engine, "hero_cube_mesh")
        pos_before = comp.get_position()

        engine.call("engine.setMode", {"mode": "play"})
        engine.wait_frame()
        comp.set_position([99.0, 99.0, 99.0])
        engine.wait_frame(10)

        engine.call("engine.setMode", {"mode": "edit"})
        engine.wait_frame(2)

        pos_after = comp.get_position()
        assertions.assert_position_close(
            pos_after, pos_before,
            tolerance=0.01, label="edit-mode position after play roundtrip",
        )


# ---------------------------------------------------------------------------
# Editor camera
# ---------------------------------------------------------------------------

class TestEditorCamera:
    """Verify editor.camera.set propagates to render.camera.data.

    This is a model→render pipeline test: the editor sets camera params,
    and we read back the render-layer camera to confirm propagation.
    """

    def test_set_camera_propagates_to_render(self, engine):
        """editor.camera.set position/pitch/yaw → render.camera.data reflects it."""
        target = {"position": [5.0, 10.0, -3.0], "pitch": -45.0, "yaw": 90.0}
        engine.call("editor.camera.set", target)
        engine.wait_frame()

        cam = engine.call("render.camera.data")
        pos = cam["position"]
        assertions.assert_position_close(
            pos, target["position"], tolerance=0.5, label="editor camera",
        )

        engine.call("editor.camera.set", TEST_CAMERA)
        engine.wait_frame()

    def test_camera_restore_to_default(self, engine):
        """After moving camera far away, restoring TEST_CAMERA brings it back."""
        engine.call("editor.camera.set", {
            "position": [100.0, 100.0, 100.0], "pitch": 0.0, "yaw": 0.0,
        })
        engine.wait_frame()

        engine.call("editor.camera.set", TEST_CAMERA)
        engine.wait_frame()

        cam = engine.call("render.camera.data")
        pos = cam["position"]
        assertions.assert_position_close(
            pos, TEST_CAMERA["position"], tolerance=0.5, label="restored camera",
        )

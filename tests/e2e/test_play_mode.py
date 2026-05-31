"""Play-mode toggle — verify enter/exit/re-enter doesn't crash.

Exercises the F5 path: editor.setMode play → edit → play.
Runs against the cubes level which already has a player_0 game object.
"""
import pytest
from root import game_object
from . import assertions

# Pre-existing (surviving) object in the default test level.
HERO = "hero_cube"
HERO_MESH = "hero_cube_mesh"
MESH_ID = "cube_mesh"
MATERIAL_ID = "mt_toon"


def _render_pos(engine, render_id):
    return engine.call("render.object.data", {"id": render_id})["gpu_data"]["obj_pos"]


class TestPlayModeToggle:
    """Enter and exit play mode multiple times without crash."""

    def test_enter_play_mode(self, engine):
        """Enter play mode — engine stays alive."""
        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)
        result = engine.call("engine.getMode")
        assert result["mode"] == "play"

        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

    def test_play_edit_roundtrip(self, engine):
        """play → edit — engine stays alive, mode restored."""
        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)

        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)
        result = engine.call("engine.getMode")
        assert result["mode"] == "edit"

    def test_double_toggle(self, engine):
        """play → edit → play → edit — the second cycle is where the crash was."""
        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)

        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)
        result = engine.call("engine.getMode")
        assert result["mode"] == "play"

        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)
        result = engine.call("engine.getMode")
        assert result["mode"] == "edit"

    def test_scene_objects_survive_toggle(self, engine):
        """All level objects still present after play→edit roundtrip."""
        root_before = engine.call("model.scene.getRoot")
        ids_before = {c["id"] for c in root_before.get("children", [])}

        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)
        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        root_after = engine.call("model.scene.getRoot")
        ids_after = {c["id"] for c in root_after.get("children", [])}

        missing = ids_before - ids_after
        assert not missing, f"Objects lost after play toggle: {missing}"

    def test_runtime_objects_cleaned_on_exit(self, engine):
        """Objects spawned during play mode are destroyed on exit."""
        root_before = engine.call("model.scene.getRoot")
        ids_before = {c["id"] for c in root_before.get("children", [])}

        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)

        engine.call("model.scene.create", {"name": "play_mode_junk"})
        engine.wait_frame(3)

        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        root_after = engine.call("model.scene.getRoot")
        ids_after = {c["id"] for c in root_after.get("children", [])}

        assert "play_mode_junk" not in ids_after, "Runtime object leaked into editor"
        assert ids_before == ids_after, (
            f"Scene changed after play roundtrip: added={ids_after - ids_before}, "
            f"removed={ids_before - ids_after}"
        )


class TestPlayModeSurvivorRestore:
    """A PRE-EXISTING object mutated during play is reset to its pre-play state on
    exit. This is the gap the count-based rollback could not close: it knew which
    objects were new, not what the old ones looked like (option B)."""

    def test_survivor_transform_restored_on_exit(self, engine):
        """Move hero_cube during play; on exit its transform snaps back — in both
        the model and the rendered matrix — without losing object identity."""
        go = game_object(engine, HERO)
        original = go.get_position()
        render_before = _render_pos(engine, HERO_MESH)

        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)

        # Mutate the survivor mid-play.
        go.set_position([original[0] + 7.0, original[1], original[2]])
        engine.wait_frame(3)
        moved = _render_pos(engine, HERO_MESH)
        assert abs((moved[0] - render_before[0]) - 7.0) < 0.05, "play-mode move did not apply"

        # Exit — survivor must reset to pre-play state.
        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        restored = go.get_position()
        assert abs(restored[0] - original[0]) < 0.01, (
            f"survivor model position not restored: {original} -> {restored}")
        assertions.assert_position_close(
            _render_pos(engine, HERO_MESH), render_before,
            tolerance=0.05, label="hero_cube render after rollback")


class TestPlayModeAttachedComponent:
    """A component grafted at runtime onto a PRE-EXISTING (surviving) object must be
    cleanly detached + destroyed on play exit — without corrupting the survivor's
    order_idx-based layout, leaking its render data, or tearing down the shared
    CDO/package assets the survivor still references (which would crash the next
    level load).

    Hits level::rollback()'s detach branch (orphan subtree on a surviving parent),
    distinct from test_runtime_objects_cleaned_on_exit which removes a whole object.
    """

    def _graft_onto_hero(self, engine, name, type_id, props):
        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)
        r = engine.call("model.component.add", {
            "object_id": HERO, "type_id": type_id, "name": name, "properties": props,
        })
        engine.wait_frame(3)
        return r["id"]

    def test_attached_mesh_destroyed_on_exit(self, engine):
        """Graft a mesh_component onto hero_cube during play; on exit it's gone
        (render data destroyed), hero_cube is unchanged, and the level reloads."""
        pos_before = _render_pos(engine, HERO_MESH)

        attached_id = self._graft_onto_hero(
            engine, "play_orphan_comp", "mesh_component",
            {"position": [0.0, 2.0, 0.0], "mesh_handle": MESH_ID, "material_handle": MATERIAL_ID})
        assert engine.call("render.object.data", {"id": attached_id}) is not None

        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        assert engine.call("engine.getMode")["mode"] == "edit"
        with pytest.raises(RuntimeError):
            engine.call("render.object.data", {"id": attached_id})
        assertions.assert_position_close(
            _render_pos(engine, HERO_MESH), pos_before,
            tolerance=0.05, label="hero_cube after detach")

        # The shared cube_mesh/mt_toon survived: hero_cube_mesh still renders, and
        # the level can reload (regression guard for the CDO render-destroy crash).
        root = engine.load_level_and_wait("simple_test", settle_time=1.0, timeout=12.0)
        assert root.get("children")

    def test_survivor_transform_propagates_after_detach(self, engine):
        """After graft+rollback, moving hero_cube still propagates through its
        rebuilt component layout — proves order_idx wasn't corrupted by detach."""
        self._graft_onto_hero(
            engine, "play_orphan_comp2", "point_light_component", {"position": [0.0, 2.0, 0.0]})
        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        go = game_object(engine, HERO)
        original = go.get_position()
        render_before = _render_pos(engine, HERO_MESH)
        try:
            go.set_position([original[0] + 4.0, original[1], original[2]])
            engine.wait_frame(2)
            render_after = _render_pos(engine, HERO_MESH)
            assert abs((render_after[0] - render_before[0]) - 4.0) < 0.05, (
                f"survivor transform propagation broken after detach: "
                f"{render_before} -> {render_after}")
        finally:
            go.set_position(original)
            engine.wait_frame()


class TestPlayModeDestroyedSurvivor:
    """A PRE-EXISTING object DESTROYED during play is recreated on exit (phase 3):
    rollback promotes its snapshot holders, preserving ALL ids (game_object AND
    components), and restores its pre-play property values."""

    def test_destroyed_survivor_recreated_on_exit(self, engine):
        go = game_object(engine, HERO)
        original = go.get_position()
        render_before = _render_pos(engine, HERO_MESH)

        engine.call_queued("engine.setMode", {"mode": "play"})
        engine.wait_frame(3)

        # Destroy the survivor (and its components) mid-play.
        engine.call("model.scene.delete", {"id": HERO})
        engine.wait_frame(3)
        with pytest.raises(RuntimeError):
            game_object(engine, HERO).get_position()  # gone from the model

        # Exit — the destroyed survivor must come back with the SAME ids.
        engine.call_queued("engine.setMode", {"mode": "edit"})
        engine.wait_frame(3)

        restored = game_object(engine, HERO).get_position()
        assertions.assert_position_close(
            restored, original, tolerance=0.01,
            label="hero_cube model recreated after destroy+rollback")
        # The component id survives too: hero_cube_mesh renders again at its
        # pre-play position (proves promotion preserved per-component identity).
        assertions.assert_position_close(
            _render_pos(engine, HERO_MESH), render_before, tolerance=0.05,
            label="hero_cube_mesh render recreated after destroy+rollback")

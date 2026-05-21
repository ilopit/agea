"""Transform propagation: game_object → component → render cache.

Verifies that set_position/set_rotation/set_scale/move on a game_object
propagate to child components' world positions in the render cache.
"""
from root import game_object
from . import assertions


def _find_go_and_render_id(engine):
    """Find a game_object and its renderable component ID in the render cache."""
    items = engine.call("model.list")["items"]
    render_objs = engine.call("render.object.list")
    render_ids = {o["id"] for o in render_objs["objects"]}

    for item in items:
        go_id = item["id"]
        for rid in render_ids:
            if rid.startswith(go_id):
                return go_id, rid
    return None, None


def _render_pos(engine, render_id):
    return engine.call("render.object.data", {"id": render_id})["gpu_data"]["obj_pos"]


class TestTransformPropagation:

    def test_move_propagates_to_render(self, engine):
        go_id, rid = _find_go_and_render_id(engine)
        assert go_id, "No renderable game_object found"

        go = game_object(engine, go_id)
        original = go.get_position()
        pos_before = _render_pos(engine, rid)

        delta = [3.0, 0.0, 0.0]
        go.move(delta)
        engine.wait_frame()

        pos_after = _render_pos(engine, rid)
        for i in range(3):
            shift = pos_after[i] - pos_before[i]
            assert abs(shift - delta[i]) < 0.05, (
                f"Render pos[{i}] shifted {shift}, expected {delta[i]}"
            )

        go.set_position(original)
        engine.wait_frame()

    def test_set_position_propagates_to_render(self, engine):
        go_id, rid = _find_go_and_render_id(engine)
        assert go_id

        go = game_object(engine, go_id)
        original = go.get_position()

        new_pos = [7.0, 4.0, -2.0]
        go.set_position(new_pos)
        engine.wait_frame()

        rp = _render_pos(engine, rid)
        for i in range(3):
            assert abs(rp[i] - new_pos[i]) < 5.0, (
                f"Render pos[{i}]={rp[i]} too far from {new_pos[i]}"
            )

        go.set_position(original)
        engine.wait_frame()

    def test_scale_propagates_to_render(self, engine):
        go_id, rid = _find_go_and_render_id(engine)
        assert go_id

        go = game_object(engine, go_id)
        original_scale = go.get_scale()
        original_pos = go.get_position()

        go.set_position([0.0, 0.0, 0.0])
        go.set_scale([1.0, 1.0, 1.0])
        engine.wait_frame()

        ro = engine.call("render.object.data", {"id": rid})
        model_mat_before = ro["gpu_data"]["model"]

        go.set_scale([3.0, 3.0, 3.0])
        engine.wait_frame()

        ro = engine.call("render.object.data", {"id": rid})
        model_mat_after = ro["gpu_data"]["model"]

        for i in range(3):
            diag_before = abs(model_mat_before[i * 4 + i])
            diag_after = abs(model_mat_after[i * 4 + i])
            assert diag_after > diag_before * 2.5, (
                f"Axis {i}: scale didn't propagate. Before={diag_before}, after={diag_after}"
            )

        go.set_scale(original_scale)
        go.set_position(original_pos)
        engine.wait_frame()

    def test_rotation_propagates_to_render(self, engine):
        go_id, rid = _find_go_and_render_id(engine)
        assert go_id

        go = game_object(engine, go_id)
        original_rot = go.get_rotation()

        go.set_rotation([0.0, 0.0, 0.0])
        engine.wait_frame()
        ro_before = engine.call("render.object.data", {"id": rid})
        mat_before = ro_before["gpu_data"]["model"]

        go.set_rotation([0.0, 90.0, 0.0])
        engine.wait_frame()
        ro_after = engine.call("render.object.data", {"id": rid})
        mat_after = ro_after["gpu_data"]["model"]

        assert mat_before != mat_after, "Rotation should change the model matrix"

        go.set_rotation(original_rot)
        engine.wait_frame()

    def test_independent_objects(self, engine):
        """Moving one game_object does not affect another's render position."""
        items = engine.call("model.list")["items"]
        render_objs = engine.call("render.object.list")
        render_ids = {o["id"] for o in render_objs["objects"]}

        pairs = []
        for item in items:
            go_id = item["id"]
            for rid in render_ids:
                if rid.startswith(go_id):
                    pairs.append((go_id, rid))
                    break
            if len(pairs) >= 2:
                break

        if len(pairs) < 2:
            return

        go_a = game_object(engine, pairs[0][0])
        rid_b = pairs[1][1]
        original = go_a.get_position()

        pos_b_before = _render_pos(engine, rid_b)

        go_a.move([100.0, 0.0, 0.0])
        engine.wait_frame()

        pos_b_after = _render_pos(engine, rid_b)
        assertions.assert_position_close(
            pos_b_after, pos_b_before, tolerance=0.01,
            label="unrelated object should not move",
        )

        go_a.set_position(original)
        engine.wait_frame()

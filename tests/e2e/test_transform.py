"""Transform modification via RPC, verified through model and render state."""
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

OBJECT_ID = "hero_cube_mesh"


class TestTransform:

    def test_read_initial_position(self, engine):
        xform = engine.call("model.transform.get", {"id": OBJECT_ID})
        assertions.assert_position_close(
            xform["position"], [0.0, 1.0, 0.0], label="hero_cube initial"
        )

        ro = engine.call("render.object.data", {"id": OBJECT_ID})
        assertions.assert_position_close(
            ro["gpu_data"]["obj_pos"], [0.0, 1.0, 0.0],
            tolerance=0.05, label="hero_cube render initial",
        )

    def test_move_updates_model_and_render(self, engine):
        new_pos = [5.0, 3.0, -2.0]
        engine.call("model.transform.set", {"id": OBJECT_ID, "position": new_pos})
        engine.wait_frame()

        xform = engine.call("model.transform.get", {"id": OBJECT_ID})
        assertions.assert_position_close(
            xform["position"], new_pos, label="model after move",
        )

        ro = engine.call("render.object.data", {"id": OBJECT_ID})
        assertions.assert_position_close(
            ro["gpu_data"]["obj_pos"], new_pos,
            tolerance=0.05, label="render after move",
        )
        assertions.assert_not_pink_bug(ro, OBJECT_ID)

    def test_scale_updates_model_and_render(self, engine):
        new_scale = [3.0, 3.0, 3.0]
        engine.call("model.transform.set", {"id": OBJECT_ID, "scale": new_scale})
        engine.wait_frame()

        xform = engine.call("model.transform.get", {"id": OBJECT_ID})
        for i in range(3):
            assert abs(xform["scale"][i] - new_scale[i]) < 0.01, (
                f"Model scale[{i}]: {xform['scale'][i]} != {new_scale[i]}"
            )

        ro = engine.call("render.object.data", {"id": OBJECT_ID})
        model_mat = ro["gpu_data"]["model"]
        for i in range(3):
            diag = model_mat[i * 4 + i]
            assert abs(diag) > 1.5, (
                f"Render model diagonal[{i}] = {diag}, expected ~{new_scale[i]}"
            )

    def test_move_does_not_corrupt_other_objects(self, engine):
        engine.call(
            "model.transform.set", {"id": OBJECT_ID, "position": [10.0, 0.0, 0.0]}
        )
        engine.wait_frame()

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

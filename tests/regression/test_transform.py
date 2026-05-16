"""Transform modification via RPC, verified through render state."""
from . import assertions


class TestTransform:

    def test_read_initial_position(self, engine):
        xform = engine.call("model.transform.get", {"id": "hero_cube_mesh"})
        assertions.assert_position_close(
            xform["position"], [0.0, 1.0, 0.0], label="hero_cube initial"
        )

    def test_move_object_updates_render_state(self, engine):
        new_pos = [5.0, 3.0, -2.0]
        engine.call("model.transform.set", {"id": "hero_cube_mesh", "position": new_pos})

        ro = engine.call("render.state.object", {"id": "hero_cube_mesh"})
        assertions.assert_position_close(
            ro["gpu_data"]["obj_pos"], new_pos, tolerance=0.05, label="hero_cube moved"
        )

    def test_move_preserves_material(self, engine):
        engine.call(
            "model.transform.set", {"id": "hero_cube_mesh", "position": [10.0, 0.0, 0.0]}
        )
        ro = engine.call("render.state.object", {"id": "hero_cube_mesh"})
        assertions.assert_not_pink_bug(ro, "hero_cube_mesh")

    def test_scale_updates_transform(self, engine):
        new_scale = [3.0, 3.0, 3.0]
        engine.call(
            "model.transform.set", {"id": "hero_cube_mesh", "scale": new_scale}
        )

        xform = engine.call("model.transform.get", {"id": "hero_cube_mesh"})
        for i in range(3):
            assert abs(xform["scale"][i] - new_scale[i]) < 0.01

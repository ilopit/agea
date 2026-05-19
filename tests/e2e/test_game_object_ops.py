"""Game object operations — transform, visibility, mesh swap."""
import pytest
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

GAME_OBJECT = "hero_cube"
MESH_COMPONENT = "hero_cube_mesh"
INITIAL_POS = [0.0, 1.0, 0.0]
INITIAL_SCALE = [1.0, 1.0, 1.0]
ALT_MESH = "plane_mesh"


def _get_render_object(engine, obj_id):
    return engine.call("render.object.data", {"id": obj_id})


class TestGameObjectPosition:

    def test_set_position_updates_model_and_render(self, engine, slow):
        new_pos = [3.0, 2.0, -1.0]
        engine.call("model.transform.set", {"id": MESH_COMPONENT, "position": new_pos})
        engine.wait_frame()
        slow("position moved")

        xform = engine.call("model.transform.get", {"id": MESH_COMPONENT})
        assertions.assert_position_close(xform["position"], new_pos, label="model")

        ro = _get_render_object(engine, MESH_COMPONENT)
        assertions.assert_position_close(
            ro["gpu_data"]["obj_pos"], new_pos, tolerance=0.05, label="render",
        )
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

    def test_set_position_restore(self, engine, slow):
        engine.call("model.transform.set", {"id": MESH_COMPONENT, "position": [10.0, 5.0, 0.0]})
        engine.wait_frame()
        slow("moved far")

        engine.call("model.transform.set", {"id": MESH_COMPONENT, "position": INITIAL_POS})
        engine.wait_frame()
        slow("restored")

        xform = engine.call("model.transform.get", {"id": MESH_COMPONENT})
        assertions.assert_position_close(xform["position"], INITIAL_POS, label="restored model")


class TestGameObjectScale:

    def test_set_scale_updates_model_and_render(self, engine, slow):
        new_scale = [3.0, 3.0, 3.0]
        engine.call("model.transform.set", {"id": MESH_COMPONENT, "scale": new_scale})
        engine.wait_frame()
        slow("scaled up 3x")

        xform = engine.call("model.transform.get", {"id": MESH_COMPONENT})
        for i in range(3):
            assert abs(xform["scale"][i] - new_scale[i]) < 0.01, (
                f"Model scale[{i}]: {xform['scale'][i]} != {new_scale[i]}"
            )

        ro = _get_render_object(engine, MESH_COMPONENT)
        model_mat = ro["gpu_data"]["model"]
        for i in range(3):
            diag = model_mat[i * 4 + i]
            assert abs(diag) > 1.5, (
                f"Render model diagonal[{i}] = {diag}, expected ~{new_scale[i]}"
            )
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

    def test_scale_restore(self, engine, slow):
        engine.call("model.transform.set", {"id": MESH_COMPONENT, "scale": [5.0, 0.2, 5.0]})
        engine.wait_frame()
        slow("flattened")

        engine.call("model.transform.set", {"id": MESH_COMPONENT, "scale": INITIAL_SCALE})
        engine.wait_frame()
        slow("restored")

        xform = engine.call("model.transform.get", {"id": MESH_COMPONENT})
        for i in range(3):
            assert abs(xform["scale"][i] - INITIAL_SCALE[i]) < 0.01


class TestGameObjectRotation:

    def test_set_rotation_updates_model(self, engine, slow):
        new_rot = [0.0, 45.0, 0.0]
        engine.call("model.transform.set", {"id": MESH_COMPONENT, "rotation": new_rot})
        engine.wait_frame()
        slow("rotated 45 degrees")

        xform = engine.call("model.transform.get", {"id": MESH_COMPONENT})
        for i in range(3):
            assert abs(xform["rotation"][i] - new_rot[i]) < 0.5, (
                f"Model rotation[{i}]: {xform['rotation'][i]} != {new_rot[i]}"
            )

        ro = _get_render_object(engine, MESH_COMPONENT)
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

    def test_rotation_restore(self, engine, slow):
        engine.call("model.transform.set", {"id": MESH_COMPONENT, "rotation": [30.0, 60.0, 0.0]})
        engine.wait_frame()
        slow("rotated")

        engine.call("model.transform.set", {"id": MESH_COMPONENT, "rotation": [0.0, 0.0, 0.0]})
        engine.wait_frame()
        slow("restored")

        xform = engine.call("model.transform.get", {"id": MESH_COMPONENT})
        for i in range(3):
            assert abs(xform["rotation"][i]) < 0.5


class TestGameObjectVisibility:

    def test_hide_clears_visible_flag(self, engine, slow):
        ro_before = _get_render_object(engine, MESH_COMPONENT)
        assert ro_before["layer_flags"] & 1, "hero_cube should have LAYER_VISIBLE initially"

        engine.call("model.visibility.set", {"id": MESH_COMPONENT, "visible": False})
        engine.wait_frame()
        slow("hidden")

        ro = _get_render_object(engine, MESH_COMPONENT)
        assert not (ro["layer_flags"] & 1), "LAYER_VISIBLE should be cleared after hide"

        # restore
        engine.call("model.visibility.set", {"id": MESH_COMPONENT, "visible": True})
        engine.wait_frame()
        slow("visible again")

        ro = _get_render_object(engine, MESH_COMPONENT)
        assert ro["layer_flags"] & 1, "LAYER_VISIBLE should be set after restore"

    def test_hide_does_not_corrupt_other_objects(self, engine, slow):
        engine.call("model.visibility.set", {"id": MESH_COMPONENT, "visible": False})
        engine.wait_frame()
        slow("hidden")

        for comp_id in EXPECTED_RENDER_OBJECTS:
            if comp_id == MESH_COMPONENT:
                continue
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        engine.call("model.visibility.set", {"id": MESH_COMPONENT, "visible": True})
        engine.wait_frame()


class TestMeshComponentSwap:

    @pytest.mark.xfail(reason="needs dedicated RPC — pointer properties not settable via properties.set")
    def test_swap_mesh_updates_render(self, engine, slow):
        ro_before = _get_render_object(engine, MESH_COMPONENT)
        original_mesh = ro_before["mesh"]["id"]
        assert original_mesh != ALT_MESH

        slow("before mesh swap")

        engine.call("model.properties.set", {
            "owner_id": MESH_COMPONENT,
            "name": "mesh",
            "value": ALT_MESH,
        })
        engine.wait_frame()
        engine.wait_frame()
        slow("mesh swapped to plane")

        ro = _get_render_object(engine, MESH_COMPONENT)
        assert ro["mesh"]["id"] == ALT_MESH, (
            f"Render mesh not updated: got {ro['mesh']['id']}, expected {ALT_MESH}"
        )
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

        # restore
        engine.call("model.properties.set", {
            "owner_id": MESH_COMPONENT,
            "name": "mesh",
            "value": original_mesh,
        })
        engine.wait_frame()
        engine.wait_frame()
        slow("mesh restored")

        ro = _get_render_object(engine, MESH_COMPONENT)
        assert ro["mesh"]["id"] == original_mesh

    def test_swap_mesh_does_not_corrupt_scene(self, engine, slow):
        engine.call("model.properties.set", {
            "owner_id": MESH_COMPONENT,
            "name": "mesh",
            "value": ALT_MESH,
        })
        engine.wait_frame()
        engine.wait_frame()
        slow("mesh swapped")

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        # restore
        engine.call("model.properties.set", {
            "owner_id": MESH_COMPONENT,
            "name": "mesh",
            "value": "cube_mesh",
        })
        engine.wait_frame()
        engine.wait_frame()

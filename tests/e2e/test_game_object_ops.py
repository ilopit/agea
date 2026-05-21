"""Game object operations — transform, visibility, mesh swap."""
import pytest
from root import game_object_component
from base import mesh_component
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

GAME_OBJECT = "hero_cube"
MESH_COMPONENT = "hero_cube_mesh"
INITIAL_POS = [0.0, 1.0, 0.0]
INITIAL_SCALE = [1.0, 1.0, 1.0]
ALT_MESH = "plane_mesh"


class TestGameObjectPosition:

    def test_set_position_updates_model_and_render(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        new_pos = [3.0, 2.0, -1.0]
        comp.set_position(new_pos)
        slow("position moved")

        pos = comp.get_position()
        assertions.assert_position_close(pos, new_pos, label="model")

        ro = comp.get_render_data()
        assertions.assert_position_close(
            ro["gpu_data"]["obj_pos"], new_pos, tolerance=0.05, label="render",
        )
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

    def test_set_position_restore(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        comp.set_position([10.0, 5.0, 0.0])
        slow("moved far")

        comp.set_position(INITIAL_POS)
        slow("restored")

        pos = comp.get_position()
        assertions.assert_position_close(pos, INITIAL_POS, label="restored model")


class TestGameObjectScale:

    def test_set_scale_updates_model_and_render(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        new_scale = [3.0, 3.0, 3.0]
        comp.set_scale(new_scale)
        slow("scaled up 3x")

        scale = comp.get_scale()
        for i in range(3):
            assert abs(scale[i] - new_scale[i]) < 0.01, (
                f"Model scale[{i}]: {scale[i]} != {new_scale[i]}"
            )

        ro = comp.get_render_data()
        model_mat = ro["gpu_data"]["model"]
        for i in range(3):
            diag = model_mat[i * 4 + i]
            assert abs(diag) > 1.5, (
                f"Render model diagonal[{i}] = {diag}, expected ~3.0"
            )
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

    def test_scale_restore(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        comp.set_scale([5.0, 0.2, 5.0])
        slow("flattened")

        comp.set_scale(INITIAL_SCALE)
        slow("restored")

        scale = comp.get_scale()
        for i in range(3):
            assert abs(scale[i] - INITIAL_SCALE[i]) < 0.01


class TestGameObjectRotation:

    def test_set_rotation_updates_model(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        new_rot = [0.0, 45.0, 0.0]
        comp.set_rotation(new_rot)
        slow("rotated 45 degrees")

        rot = comp.get_rotation()
        for i in range(3):
            assert abs(rot[i] - new_rot[i]) < 0.5, (
                f"Model rotation[{i}]: {rot[i]} != {new_rot[i]}"
            )

        ro = comp.get_render_data()
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

    def test_rotation_restore(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        comp.set_rotation([30.0, 60.0, 0.0])
        slow("rotated")

        comp.set_rotation([0.0, 0.0, 0.0])
        slow("restored")

        rot = comp.get_rotation()
        for i in range(3):
            assert abs(rot[i]) < 0.5


class TestGameObjectVisibility:

    def test_hide_clears_visible_flag(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        ro_before = comp.get_render_data()
        assert ro_before["layer_flags"] & 1, "hero_cube should have LAYER_VISIBLE initially"

        comp.set_visible(False)
        slow("hidden")

        ro = comp.get_render_data()
        assert not (ro["layer_flags"] & 1), "LAYER_VISIBLE should be cleared after hide"

        comp.set_visible(True)
        slow("visible again")

        ro = comp.get_render_data()
        assert ro["layer_flags"] & 1, "LAYER_VISIBLE should be set after restore"

    def test_hide_does_not_corrupt_other_objects(self, engine, slow):
        comp = game_object_component(engine, MESH_COMPONENT)
        comp.set_visible(False)
        slow("hidden")

        for comp_id in EXPECTED_RENDER_OBJECTS:
            if comp_id == MESH_COMPONENT:
                continue
            other = game_object_component(engine, comp_id)
            ro = other.get_render_data()
            assertions.assert_not_pink_bug(ro, comp_id)

        comp.set_visible(True)


class TestMeshComponentSwap:

    def test_swap_mesh_updates_render(self, engine, slow):
        mc = mesh_component(engine, MESH_COMPONENT)
        ro_before = mc.get_render_data()
        original_mesh = ro_before["mesh"]["id"]
        assert original_mesh != ALT_MESH

        slow("before mesh swap")

        mc.set_mesh(ALT_MESH)
        engine.wait_frame()
        slow("mesh swapped to plane")

        ro = mc.get_render_data()
        assert ro["mesh"]["id"] == ALT_MESH, (
            f"Render mesh not updated: got {ro['mesh']['id']}, expected {ALT_MESH}"
        )
        assertions.assert_not_pink_bug(ro, MESH_COMPONENT)

        # restore
        mc.set_mesh(original_mesh)
        engine.wait_frame()
        slow("mesh restored")

        ro = mc.get_render_data()
        assert ro["mesh"]["id"] == original_mesh

    def test_swap_mesh_does_not_corrupt_scene(self, engine, slow):
        mc = mesh_component(engine, MESH_COMPONENT)
        mc.set_mesh(ALT_MESH)
        engine.wait_frame()
        slow("mesh swapped")

        from root import game_object_component as goc
        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = goc(engine, comp_id).get_render_data()
            assertions.assert_not_pink_bug(ro, comp_id)

        # restore
        mc.set_mesh("cube_mesh")
        engine.wait_frame()

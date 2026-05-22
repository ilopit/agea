"""Game object — transforms, visibility, mesh swap, reflected functions,
component chain, component lifecycle, tickable, animated mesh.

Two code paths mutate transforms:
- Property path: game_object_component.set_position() → model.object.property.set
- Invoke path:   game_object.set_position()            → model.object.function.invoke

Both end up calling the same C++ setter, but exercise different RPC→model
pipelines. TestTransform covers both and verifies render propagation.
"""
import pytest
from root import game_object, game_object_component, smart_object
from base import animated_mesh_component
from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions
from .property_helpers import (
    create_test_object, add_component, cleanup_object,
    get_property, set_property,
)

# hero_cube is a mesh_object — visible in viewport for visual debugging
HERO = "hero_cube"
HERO_MESH = "hero_cube_mesh"
INITIAL_POS = [0.0, 1.0, 0.0]
INITIAL_SCALE = [1.0, 1.0, 1.0]

CHAIN_LENGTH = 3
MESH_ID = "cube_mesh"
MATERIAL_ID = "mt_toon"
LOCAL_OFFSET = [0.0, 2.0, 0.0]
CHAIN_GO = "test_chain_go"

DYNAMIC_GO = "test_dynamic_go"


def _render_pos(engine, render_id):
    return engine.call("render.object.data", {"id": render_id})["gpu_data"]["obj_pos"]


def _render_model_matrix(engine, render_id):
    return engine.call("render.object.data", {"id": render_id})["gpu_data"]["model"]


# ---------------------------------------------------------------------------
# Transform — both property and invoke paths, verified against render cache
# ---------------------------------------------------------------------------

class TestTransform:
    """Set position/rotation/scale on hero_cube, verify model reads back correctly
    and render.object.data reflects the change in GPU data (obj_pos, model matrix).

    Tests use the invoke path (game_object.set_position) since that's what MCP
    and editor UI call. The property path (game_object_component.set_position)
    is tested once to confirm both pipelines converge.
    """

    def test_position_invoke_model_and_render(self, engine, slow):
        """Invoke set_position → model reads back, render shifts by same delta."""
        go = game_object(engine, HERO)
        original = go.get_position()
        render_before = _render_pos(engine, HERO_MESH)

        new_pos = [5.0, 3.0, -2.0]
        go.set_position(new_pos)
        engine.wait_frame()
        slow("position -> [5, 3, -2]")

        assertions.assert_position_close(go.get_position(), new_pos, label="model")
        render_after = _render_pos(engine, HERO_MESH)
        for i in range(3):
            expected_shift = new_pos[i] - original[i]
            actual_shift = render_after[i] - render_before[i]
            assert abs(actual_shift - expected_shift) < 0.05, (
                f"render shift[{i}]: {actual_shift} vs expected {expected_shift}"
            )

        go.set_position(INITIAL_POS)
        engine.wait_frame()
        slow("restored")

    def test_position_property_model_and_render(self, engine, slow):
        """Property set_position → same result as invoke path."""
        comp = game_object_component(engine, HERO_MESH)
        new_pos = [3.0, 2.0, -1.0]
        comp.set_position(new_pos)
        slow("property position -> [3, 2, -1]")

        assertions.assert_position_close(comp.get_position(), new_pos, label="model")
        assertions.assert_position_close(
            _render_pos(engine, HERO_MESH), new_pos, tolerance=0.05, label="render",
        )

        comp.set_position(INITIAL_POS)
        slow("restored")

    def test_scale_model_and_render(self, engine, slow):
        """set_scale(3x) → render model matrix diagonals grow."""
        go = game_object(engine, HERO)
        original = go.get_scale()
        go.set_scale([3.0, 3.0, 3.0])
        engine.wait_frame()
        slow("scaled 3×")

        scale = go.get_scale()
        for i in range(3):
            assert abs(scale[i] - 3.0) < 0.01

        mat = _render_model_matrix(engine, HERO_MESH)
        for i in range(3):
            assert abs(mat[i * 4 + i]) > 1.5, f"diagonal[{i}]={mat[i*4+i]}, expected ~3"

        go.set_scale(original)
        engine.wait_frame()
        slow("restored")

    def test_rotation_model_and_render(self, engine, slow):
        """set_rotation(45° Y) → model reads back, render matrix changes."""
        go = game_object(engine, HERO)
        original = go.get_rotation()
        mat_before = _render_model_matrix(engine, HERO_MESH)

        go.set_rotation([0.0, 45.0, 0.0])
        engine.wait_frame()
        slow("rotated 45° Y")

        rot = go.get_rotation()
        assert abs(rot[1] - 45.0) < 0.5

        mat_after = _render_model_matrix(engine, HERO_MESH)
        assert mat_before != mat_after, "Rotation should change the model matrix"

        go.set_rotation(original)
        engine.wait_frame()
        slow("restored")

    def test_move_does_not_corrupt_other_objects(self, engine, slow):
        """Moving hero_cube doesn't cause pink bug on any other render object."""
        go = game_object(engine, HERO)
        original = go.get_position()
        go.set_position([10.0, 0.0, 0.0])
        engine.wait_frame()
        slow("moved far right")

        for comp_id in EXPECTED_RENDER_OBJECTS:
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        go.set_position(original)
        engine.wait_frame()

    def test_independent_objects(self, engine):
        """Moving hero_cube does not shift ground's render position."""
        go = game_object(engine, HERO)
        original = go.get_position()
        ground_before = _render_pos(engine, "ground_mesh")

        go.set_position([100.0, 0.0, 0.0])
        engine.wait_frame()

        ground_after = _render_pos(engine, "ground_mesh")
        assertions.assert_position_close(
            ground_after, ground_before, tolerance=0.01,
            label="ground should not move when hero moves",
        )

        go.set_position(original)
        engine.wait_frame()


# ---------------------------------------------------------------------------
# Reflected functions — invoke-only behaviors (direction vectors, move, meta)
# ---------------------------------------------------------------------------

class TestFunctions:
    """Test reflected functions that aren't simple property setters:
    direction vectors (forward/up/right), relative move(), quaternion access,
    and type metadata introspection.
    """

    def test_direction_vectors_are_unit_length(self, engine):
        """get_forward/up/right_vector all return unit-length vec3."""
        go = game_object(engine, HERO)
        for name in ("get_forward_vector", "get_up_vector", "get_right_vector"):
            vec = getattr(go, name)()
            assert len(vec) == 3
            length = sum(c * c for c in vec) ** 0.5
            assert abs(length - 1.0) < 0.01, f"{name} not unit length: {length}"

    def test_move_is_relative(self, engine):
        """move() accumulates — two moves add up, not overwrite."""
        go = game_object(engine, HERO)
        original = go.get_position()

        go.move([1.0, 0.0, 0.0])
        engine.wait_frame()
        expected = [original[0] + 1.0, original[1], original[2]]
        assertions.assert_position_close(go.get_position(), expected, label="after +1x")

        go.move([0.0, 2.0, 0.0])
        engine.wait_frame()
        expected[1] += 2.0
        assertions.assert_position_close(go.get_position(), expected, label="after +2y")

        go.set_position(original)
        engine.wait_frame()

    def test_get_rotation_quat_in_meta(self, engine):
        """get_rotation_quat exists in type meta and is invocable."""
        meta = engine.get_type_meta("game_object")
        funcs = {f["name"]: f for f in meta["functions"]}
        assert "get_rotation_quat" in funcs
        assert funcs["get_rotation_quat"]["invocable"] is True

    def test_get_reflection_in_meta(self, engine):
        """get_reflection exists in smart_object type meta."""
        meta = engine.get_type_meta("smart_object")
        funcs = {f["name"]: f for f in meta["functions"]}
        assert "get_reflection" in funcs

    def test_game_object_functions_complete(self, engine):
        """game_object type meta contains all expected reflected functions."""
        meta = engine.get_type_meta("game_object")
        funcs = {f["name"]: f for f in meta["functions"]}

        for name in ("get_position", "set_position", "move",
                     "get_forward_vector", "get_rotation_quat"):
            assert name in funcs, f"{name} missing from game_object functions"

        assert funcs["get_position"]["invocable"] is True

    def test_mesh_object_inherits_functions(self, engine):
        """mesh_object inherits game_object functions (get_position, move)."""
        meta = engine.get_type_meta("mesh_object")
        func_names = {f["name"] for f in meta.get("functions", [])}
        assert "get_position" in func_names
        assert "move" in func_names


# ---------------------------------------------------------------------------
# Visibility — layer_flags in render cache
# ---------------------------------------------------------------------------

class TestVisibility:
    """Toggle LAYER_VISIBLE flag on hero_cube's component and verify
    the render cache reflects the change without corrupting other objects.
    """

    def test_hide_and_show(self, engine, slow):
        """hide → LAYER_VISIBLE cleared; show → LAYER_VISIBLE restored."""
        comp = game_object_component(engine, HERO_MESH)
        ro = comp.get_render_data()
        assert ro["layer_flags"] & 1, "hero_cube should start visible"

        comp.set_visible(False)
        slow("hidden")
        assert not (comp.get_render_data()["layer_flags"] & 1)

        comp.set_visible(True)
        slow("visible again")
        assert comp.get_render_data()["layer_flags"] & 1

    def test_hide_does_not_corrupt_others(self, engine):
        """Hiding hero_cube doesn't cause pink bug on ground."""
        comp = game_object_component(engine, HERO_MESH)
        comp.set_visible(False)

        for comp_id in EXPECTED_RENDER_OBJECTS:
            if comp_id == HERO_MESH:
                continue
            ro = engine.call("render.object.data", {"id": comp_id})
            assertions.assert_not_pink_bug(ro, comp_id)

        comp.set_visible(True)




# ---------------------------------------------------------------------------
# Component chain — nested parent→child transform hierarchy
# ---------------------------------------------------------------------------

def _create_chain(engine):
    """Create game_object with a chain of mesh_components, each offset by LOCAL_OFFSET."""
    engine.call("model.scene.create", {"name": CHAIN_GO})
    comp_ids = []
    parent_id = None
    for i in range(CHAIN_LENGTH):
        params = {
            "object_id": CHAIN_GO,
            "type_id": "mesh_component",
            "name": f"chain_{i}",
            "properties": {
                "position": LOCAL_OFFSET,
                "mesh_handle": MESH_ID,
                "material_handle": MATERIAL_ID,
            },
        }
        if parent_id:
            params["parent_id"] = parent_id
        r = engine.call("model.component.add", params)
        comp_ids.append(r["id"])
        parent_id = r["id"]
    engine.wait_frame()
    return comp_ids


class TestComponentChain:
    """Build a 3-deep parent→child mesh_component chain on a dynamic game_object.
    Verify local offsets accumulate in world space, and that parent transforms
    (move, scale, rotate) propagate correctly through the hierarchy.
    """

    def test_chain_visible_in_render(self, engine):
        """All 3 chain components appear in render cache with valid materials."""
        comp_ids = _create_chain(engine)
        try:
            engine.wait_frame(3)
            for comp_id in comp_ids:
                ro = engine.call("render.object.data", {"id": comp_id})
                assert ro is not None, f"{comp_id} not in render cache"
                assertions.assert_not_pink_bug(ro, comp_id)
        finally:
            cleanup_object(engine, CHAIN_GO)

    def test_local_offsets_accumulate(self, engine):
        """Child at depth N has world Y = LOCAL_OFFSET.y × N."""
        comp_ids = _create_chain(engine)
        try:
            for i, comp_id in enumerate(comp_ids):
                depth = i + 1
                expected_y = LOCAL_OFFSET[1] * depth
                assertions.assert_position_close(
                    _render_pos(engine, comp_id), [0.0, expected_y, 0.0],
                    tolerance=0.1, label=f"chain[{i}] depth={depth}",
                )
        finally:
            cleanup_object(engine, CHAIN_GO)

    def test_move_parent_shifts_chain(self, engine):
        """Moving the parent game_object shifts all children equally."""
        comp_ids = _create_chain(engine)
        try:
            positions_before = [_render_pos(engine, c) for c in comp_ids]
            go = game_object(engine, CHAIN_GO)
            delta = [5.0, 0.0, 3.0]
            go.move(delta)
            engine.wait_frame()

            for i, comp_id in enumerate(comp_ids):
                expected = [positions_before[i][j] + delta[j] for j in range(3)]
                assertions.assert_position_close(
                    _render_pos(engine, comp_id), expected,
                    tolerance=0.1, label=f"chain[{i}] after move",
                )
        finally:
            cleanup_object(engine, CHAIN_GO)

    def test_scale_parent_scales_offsets(self, engine):
        """Scaling parent 2× doubles children's world-space offsets."""
        comp_ids = _create_chain(engine)
        try:
            go = game_object(engine, CHAIN_GO)
            go.set_scale([2.0, 2.0, 2.0])
            engine.wait_frame()

            for i, comp_id in enumerate(comp_ids):
                depth = i + 1
                expected_y = LOCAL_OFFSET[1] * depth * 2.0
                assertions.assert_position_close(
                    _render_pos(engine, comp_id), [0.0, expected_y, 0.0],
                    tolerance=0.2, label=f"chain[{i}] after 2× scale",
                )
        finally:
            cleanup_object(engine, CHAIN_GO)

    def test_rotation_parent_rotates_chain(self, engine):
        """90° X rotation swaps children's Y offsets into Z."""
        comp_ids = _create_chain(engine)
        try:
            go = game_object(engine, CHAIN_GO)
            go.set_rotation([90.0, 0.0, 0.0])
            engine.wait_frame()

            for i, comp_id in enumerate(comp_ids):
                depth = i + 1
                pos = _render_pos(engine, comp_id)
                expected_z = LOCAL_OFFSET[1] * depth
                assert abs(pos[1]) < 0.2, f"chain[{i}] Y should be ~0, got {pos[1]}"
                assert abs(pos[2] - expected_z) < 0.2, f"chain[{i}] Z should be ~{expected_z}, got {pos[2]}"
        finally:
            cleanup_object(engine, CHAIN_GO)


# ---------------------------------------------------------------------------
# Component lifecycle — dynamic creation and deletion via RPC
# ---------------------------------------------------------------------------

class TestComponentLifecycle:
    """Create game_objects dynamically, add components of each type, verify
    they appear in the model/render layer, then delete and verify cleanup.
    """

    def test_list_component_types(self, engine):
        """model.component.listTypes includes all expected component types."""
        result = engine.call("model.component.listTypes")
        if isinstance(result, list):
            type_ids = {t.get("type_id", t.get("id", "")) for t in result}
        else:
            type_ids = {t.get("type_id", t.get("id", "")) for t in result.get("types", [])}
        for expected in ("mesh_component", "directional_light_component",
                         "point_light_component", "spot_light_component"):
            assert expected in type_ids, f"{expected} missing from: {type_ids}"

    def test_add_mesh_component_appears_in_render(self, engine):
        """Dynamically added mesh_component is visible in render cache."""
        create_test_object(engine, DYNAMIC_GO)
        try:
            comp_id = add_component(engine, DYNAMIC_GO, "mesh_component", "dyn_mesh",
                                    props={"mesh_handle": MESH_ID, "material_handle": MATERIAL_ID})
            engine.wait_frame(2)
            ro = engine.call("render.object.data", {"id": comp_id})
            assert ro is not None
            assertions.assert_not_pink_bug(ro, comp_id)
        finally:
            cleanup_object(engine, DYNAMIC_GO)

    def test_add_light_components(self, engine):
        """Dynamically added light components have readable properties."""
        create_test_object(engine, DYNAMIC_GO)
        try:
            for type_id in ("directional_light_component", "point_light_component",
                            "spot_light_component"):
                comp_id = add_component(engine, DYNAMIC_GO, type_id, f"dyn_{type_id}")
                props = engine.call("model.object.property.get", {"id": comp_id})
                assert props is not None, f"{type_id} returned no properties"
        finally:
            cleanup_object(engine, DYNAMIC_GO)

    def test_delete_object_removes_from_scene(self, engine):
        """Deleting a game_object removes it from the scene graph."""
        create_test_object(engine, DYNAMIC_GO)
        add_component(engine, DYNAMIC_GO, "mesh_component", "dyn_mesh2",
                      props={"mesh_handle": MESH_ID, "material_handle": MATERIAL_ID})
        engine.wait_frame(3)

        engine.call("model.scene.delete", {"id": DYNAMIC_GO})
        engine.wait_frame(3)

        root = engine.call("model.scene.getRoot")
        obj_ids = {c["id"] for c in root["children"]}
        assert DYNAMIC_GO not in obj_ids


# ---------------------------------------------------------------------------
# Tickable — boolean property on game_object_component
# ---------------------------------------------------------------------------

class TestTickable:
    """Verify the tickable boolean property round-trips through property.set/get."""

    def test_tickable_roundtrip(self, engine):
        """Toggle tickable → read back matches → restore original."""
        original = get_property(engine, HERO_MESH, "tickable")
        assert isinstance(original, bool)

        set_property(engine, HERO_MESH, "tickable", not original)
        assert get_property(engine, HERO_MESH, "tickable") == (not original)

        set_property(engine, HERO_MESH, "tickable", original)


# ---------------------------------------------------------------------------
# Animated mesh — non-buffer property round-trips on dynamic component
# ---------------------------------------------------------------------------

class TestAnimatedMesh:
    """Create an animated_mesh_component dynamically and verify its scalar/bool/id
    properties round-trip through the property system. Skips m_gltf (buffer type,
    not practical to set via RPC).
    """

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "animated_mesh_component", "dyn_anim")
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_playback_speed(self, engine):
        amc = animated_mesh_component(engine, self.comp_id)
        amc.set_playback_speed(2.0)
        assert abs(amc.get_playback_speed() - 2.0) < 0.01

    def test_looping(self, engine):
        amc = animated_mesh_component(engine, self.comp_id)
        amc.set_looping(False)
        assert amc.get_looping() is False
        amc.set_looping(True)
        assert amc.get_looping() is True

    def test_playing(self, engine):
        amc = animated_mesh_component(engine, self.comp_id)
        amc.set_playing(False)
        assert amc.get_playing() is False

    def test_clip_name(self, engine):
        amc = animated_mesh_component(engine, self.comp_id)
        amc.set_clip_name("idle")
        assert amc.get_clip_name() == "idle"

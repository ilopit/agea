"""Nested mesh_component chain — create via component_add with construct params,
verify transform propagation through parent→child→grandchild."""
from root import game_object
from . import assertions

CHAIN_LENGTH = 3
MESH_ID = "cube_mesh"
MATERIAL_ID = "mt_toon"
LOCAL_OFFSET = [0.0, 2.0, 0.0]
GO_NAME = "test_chain_go"


def _create_chain(engine):
    """Create a game_object with a chain of mesh_components, each offset by LOCAL_OFFSET."""
    engine.call("model.scene.create", {"name": GO_NAME})

    comp_ids = []
    parent_id = None
    for i in range(CHAIN_LENGTH):
        params = {
            "object_id": GO_NAME,
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


def _cleanup(engine):
    try:
        engine.call("model.scene.delete", {"id": GO_NAME})
        engine.wait_frame()
    except RuntimeError:
        pass


def _render_pos(engine, comp_id):
    return engine.call("render.object.data", {"id": comp_id})["gpu_data"]["obj_pos"]


class TestComponentChain:

    def test_chain_visible_in_render(self, engine):
        """All mesh_components in the chain should appear in the render cache."""
        comp_ids = _create_chain(engine)
        try:
            # Debug: check properties on first component
            props = engine.call("model.object.property.get", {"id": comp_ids[0]})
            owners = props.get("owners", [])
            for ow in owners:
                if ow["id"] == comp_ids[0]:
                    for cat in ow.get("categories", []):
                        for f in cat.get("fields", []):
                            if f["name"] in ("mesh", "material", "position"):
                                print(f"  {f['name']} = {f.get('value')}")

            # Extra frames for render_bridge
            engine.wait_frame(3)

            for comp_id in comp_ids:
                ro = engine.call("render.object.data", {"id": comp_id})
                assert ro is not None, f"{comp_id} not in render cache"
                assertions.assert_not_pink_bug(ro, comp_id)
        finally:
            _cleanup(engine)

    def test_local_offsets_accumulate(self, engine):
        """Each child's world Y = LOCAL_OFFSET.y * depth."""
        comp_ids = _create_chain(engine)
        try:
            for i, comp_id in enumerate(comp_ids):
                depth = i + 1
                expected_y = LOCAL_OFFSET[1] * depth
                pos = _render_pos(engine, comp_id)
                assertions.assert_position_close(
                    pos, [0.0, expected_y, 0.0],
                    tolerance=0.1,
                    label=f"chain[{i}] depth={depth}",
                )
        finally:
            _cleanup(engine)

    def test_move_parent_shifts_entire_chain(self, engine):
        """Moving the game_object shifts all chain components equally."""
        comp_ids = _create_chain(engine)
        try:
            positions_before = [_render_pos(engine, c) for c in comp_ids]

            go = game_object(engine, GO_NAME)
            delta = [5.0, 0.0, 3.0]
            go.move(delta)
            engine.wait_frame()

            for i, comp_id in enumerate(comp_ids):
                pos = _render_pos(engine, comp_id)
                expected = [positions_before[i][j] + delta[j] for j in range(3)]
                assertions.assert_position_close(
                    pos, expected, tolerance=0.1,
                    label=f"chain[{i}] after move",
                )
        finally:
            _cleanup(engine)

    def test_scale_parent_scales_chain_offsets(self, engine):
        """Scaling the game_object multiplies children's world offsets."""
        comp_ids = _create_chain(engine)
        try:
            go = game_object(engine, GO_NAME)
            go.set_scale([2.0, 2.0, 2.0])
            engine.wait_frame()

            for i, comp_id in enumerate(comp_ids):
                depth = i + 1
                expected_y = LOCAL_OFFSET[1] * depth * 2.0
                pos = _render_pos(engine, comp_id)
                assertions.assert_position_close(
                    pos, [0.0, expected_y, 0.0],
                    tolerance=0.2,
                    label=f"chain[{i}] after 2x scale",
                )
        finally:
            _cleanup(engine)

    def test_rotation_rotates_chain(self, engine):
        """90° rotation around X swaps Y offsets into +Z."""
        comp_ids = _create_chain(engine)
        try:
            go = game_object(engine, GO_NAME)
            go.set_rotation([90.0, 0.0, 0.0])
            engine.wait_frame()

            for i, comp_id in enumerate(comp_ids):
                depth = i + 1
                pos = _render_pos(engine, comp_id)
                expected_z = LOCAL_OFFSET[1] * depth
                assert abs(pos[1]) < 0.2, (
                    f"chain[{i}] Y should be ~0 after 90° X rotation, got {pos[1]}"
                )
                assert abs(pos[2] - expected_z) < 0.2, (
                    f"chain[{i}] Z should be ~{expected_z}, got {pos[2]}"
                )
        finally:
            _cleanup(engine)

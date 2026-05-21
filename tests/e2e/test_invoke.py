"""Reflected function invocation via model.object.invoke RPC."""
from root import game_object
from . import assertions


def _find_test_object(engine):
    """Pick the first game_object in the current level."""
    items = engine.call("model.list")["items"]
    return items[0]["id"] if items else None


class TestInvokeGetters:

    def test_get_position(self, engine):
        go = game_object(engine, _find_test_object(engine))
        pos = go.get_position()
        assert len(pos) == 3, f"Expected [x y z], got {pos}"

    def test_get_rotation(self, engine):
        go = game_object(engine, _find_test_object(engine))
        rot = go.get_rotation()
        assert len(rot) == 3, f"Expected [x y z], got {rot}"

    def test_get_scale(self, engine):
        go = game_object(engine, _find_test_object(engine))
        scale = go.get_scale()
        assert len(scale) == 3, f"Expected [x y z], got {scale}"

    def test_get_forward_vector(self, engine):
        go = game_object(engine, _find_test_object(engine))
        fwd = go.get_forward_vector()
        assert len(fwd) == 3, f"Expected unit vec3, got {fwd}"
        length = sum(c * c for c in fwd) ** 0.5
        assert abs(length - 1.0) < 0.01, f"Forward vector not unit length: {length}"

    def test_get_up_vector(self, engine):
        go = game_object(engine, _find_test_object(engine))
        up = go.get_up_vector()
        length = sum(c * c for c in up) ** 0.5
        assert abs(length - 1.0) < 0.01, f"Up vector not unit length: {length}"

    def test_get_right_vector(self, engine):
        go = game_object(engine, _find_test_object(engine))
        right = go.get_right_vector()
        length = sum(c * c for c in right) ** 0.5
        assert abs(length - 1.0) < 0.01, f"Right vector not unit length: {length}"


class TestInvokeSetters:

    def test_set_position(self, engine):
        go = game_object(engine, _find_test_object(engine))
        original = go.get_position()

        new_pos = [4.0, 2.0, -1.0]
        go.set_position(new_pos)
        engine.wait_frame()

        assertions.assert_position_close(go.get_position(), new_pos, label="after set_position")

        go.set_position(original)
        engine.wait_frame()

    def test_set_rotation(self, engine):
        go = game_object(engine, _find_test_object(engine))
        original = go.get_rotation()

        new_rot = [0.0, 45.0, 0.0]
        go.set_rotation(new_rot)
        engine.wait_frame()

        assertions.assert_position_close(go.get_rotation(), new_rot, tolerance=0.5, label="after set_rotation")

        go.set_rotation(original)
        engine.wait_frame()

    def test_set_scale(self, engine):
        go = game_object(engine, _find_test_object(engine))
        original = go.get_scale()

        new_scale = [2.0, 2.0, 2.0]
        go.set_scale(new_scale)
        engine.wait_frame()

        assertions.assert_position_close(go.get_scale(), new_scale, label="after set_scale")

        go.set_scale(original)
        engine.wait_frame()


class TestInvokeMove:

    def test_move_is_relative(self, engine):
        go = game_object(engine, _find_test_object(engine))
        original = go.get_position()

        go.move([1.0, 0.0, 0.0])
        engine.wait_frame()

        expected = [original[0] + 1.0, original[1], original[2]]
        assertions.assert_position_close(go.get_position(), expected, label="after first move")

        go.move([0.0, 2.0, 0.0])
        engine.wait_frame()

        expected[1] += 2.0
        assertions.assert_position_close(go.get_position(), expected, label="after second move")

        go.set_position(original)
        engine.wait_frame()


class TestInvokeTypeMeta:

    def test_game_object_functions_in_meta(self, engine):
        meta = engine.get_type_meta("game_object")
        funcs = {f["name"]: f for f in meta["functions"]}

        assert "get_position" in funcs
        assert "set_position" in funcs
        assert "move" in funcs
        assert "get_forward_vector" in funcs

        assert funcs["get_position"]["invocable"] is True
        assert funcs["get_position"]["return_type"] == "vec3"

        assert funcs["set_position"]["invocable"] is True
        assert "args" in funcs["set_position"]

    def test_inherited_functions(self, engine):
        meta = engine.get_type_meta("mesh_object")
        func_names = {f["name"] for f in meta.get("functions", [])}
        assert "get_position" in func_names, "mesh_object should inherit game_object functions"
        assert "move" in func_names

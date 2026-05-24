"""Light transform propagation — verify point/spot light render position updates
when the parent game_object moves.
"""
import pytest
from base import point_light_component, spot_light_component
from .property_helpers import create_test_object, add_component, cleanup_object

DYNAMIC_GO = "test_lt_go"
INITIAL_POS = [0.0, 5.0, 0.0]
MOVED_POS = [10.0, 8.0, -3.0]
SPOT_DIR = [0.0, -1.0, 0.0]

TOL = 0.01
RENDER_TOL = 0.1


def _find_universal_light(engine, comp_id):
    lights = engine.call("render.lights.data")
    for entry in lights.get("universal", []):
        if entry["id"] == comp_id:
            return entry
    return None


def _assert_vec3_close(actual, expected, tol=TOL, label=""):
    ctx = f" ({label})" if label else ""
    assert len(actual) == len(expected), f"Length mismatch{ctx}: {len(actual)} vs {len(expected)}"
    for i, (a, e) in enumerate(zip(actual, expected)):
        assert abs(a - e) < tol, f"[{i}]{ctx}: got {a}, expected {e} (tol={tol})"


def _move_parent(engine, go_id, pos):
    engine.invoke(go_id, "set_position", [pos])
    engine.wait_frame()


# ---------------------------------------------------------------------------
# Point light — moves with parent
# ---------------------------------------------------------------------------

class TestPointLightTransform:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "point_light_component", "pt_xform")
        _move_parent(engine, DYNAMIC_GO, INITIAL_POS)
        plc = point_light_component(engine, self.comp_id)
        plc.set_radius(50.0)
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_initial_position_in_render(self, engine, slow):
        slow("initial position")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None, "point light missing from render"
        _assert_vec3_close(entry["position"], INITIAL_POS, tol=RENDER_TOL, label="render initial")

    def test_position_follows_parent_move(self, engine, slow):
        _move_parent(engine, DYNAMIC_GO, MOVED_POS)
        slow("parent moved")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None, "point light missing from render after move"
        _assert_vec3_close(entry["position"], MOVED_POS, tol=RENDER_TOL, label="render after move")

    def test_position_follows_multiple_moves(self, engine, slow):
        positions = [
            [3.0, 1.0, 0.0],
            [-5.0, 12.0, 7.0],
            [0.0, 0.0, 0.0],
        ]
        for pos in positions:
            _move_parent(engine, DYNAMIC_GO, pos)
            slow(f"moved to {pos}")
            entry = _find_universal_light(engine, self.comp_id)
            assert entry is not None
            _assert_vec3_close(entry["position"], pos, tol=RENDER_TOL, label=f"render at {pos}")


# ---------------------------------------------------------------------------
# Spot light — moves with parent
# ---------------------------------------------------------------------------

class TestSpotLightTransform:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "spot_light_component", "sp_xform")
        _move_parent(engine, DYNAMIC_GO, INITIAL_POS)
        slc = spot_light_component(engine, self.comp_id)
        slc.set_direction(SPOT_DIR)
        slc.set_radius(50.0)
        slc.set_cut_off(30.0)
        slc.set_outer_cut_off(45.0)
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_initial_position_in_render(self, engine, slow):
        slow("initial position")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None, "spot light missing from render"
        _assert_vec3_close(entry["position"], INITIAL_POS, tol=RENDER_TOL, label="render initial")

    def test_position_follows_parent_move(self, engine, slow):
        _move_parent(engine, DYNAMIC_GO, MOVED_POS)
        slow("parent moved")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None, "spot light missing from render after move"
        _assert_vec3_close(entry["position"], MOVED_POS, tol=RENDER_TOL, label="render after move")

    def test_position_follows_multiple_moves(self, engine, slow):
        positions = [
            [3.0, 1.0, 0.0],
            [-5.0, 12.0, 7.0],
            [0.0, 0.0, 0.0],
        ]
        for pos in positions:
            _move_parent(engine, DYNAMIC_GO, pos)
            slow(f"moved to {pos}")
            entry = _find_universal_light(engine, self.comp_id)
            assert entry is not None
            _assert_vec3_close(entry["position"], pos, tol=RENDER_TOL, label=f"render at {pos}")

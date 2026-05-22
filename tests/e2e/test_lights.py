"""Light components — directional, point, spot property coverage + render verification.

Tests model-layer property round-trips for all three light types via generated
proxies. Every roundtrip also verifies the render layer reflects the change.
"""
import math
import pytest
from base import directional_light_component, point_light_component, spot_light_component
from .conftest import EXPECTED_RENDER_OBJECTS
from .property_helpers import create_test_object, add_component, cleanup_object
from . import assertions

DYNAMIC_GO = "test_light_go"
SUN_COMPONENT_ID = "sun_lc"

LIGHT_POS = [0.0, 5.0, 0.0]
SPOT_DIR = [0.0, -1.0, 0.0]

TOL = 0.01
RENDER_TOL = 0.1


def _assert_vec3_close(actual, expected, tol=TOL, label=""):
    ctx = f" ({label})" if label else ""
    assert len(actual) == len(expected), f"Length mismatch{ctx}: {len(actual)} vs {len(expected)}"
    for i, (a, e) in enumerate(zip(actual, expected)):
        assert abs(a - e) < tol, f"[{i}]{ctx}: got {a}, expected {e} (tol={tol})"


def _find_directional_light(engine, comp_id):
    lights = engine.call("render.lights.data")
    for entry in lights.get("directional", []):
        if entry["id"] == comp_id:
            return entry
    return None


def _find_universal_light(engine, comp_id):
    lights = engine.call("render.lights.data")
    for entry in lights.get("universal", []):
        if entry["id"] == comp_id:
            return entry
    return None


def _assert_scene_not_corrupted(engine):
    for comp_id in EXPECTED_RENDER_OBJECTS:
        ro = engine.call("render.object.data", {"id": comp_id})
        assertions.assert_not_pink_bug(ro, comp_id)


def _position_light(engine, go_id):
    engine.invoke(go_id, "set_position", [LIGHT_POS])
    engine.wait_frame()


# ---------------------------------------------------------------------------
# Directional light
# ---------------------------------------------------------------------------

class TestDirectionalLight:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        # Kill the sun so the dynamic directional light is the only illumination
        self._sun = directional_light_component(engine, SUN_COMPONENT_ID)
        self._sun_diffuse = self._sun.get_diffuse()
        self._sun_ambient = self._sun.get_ambient()
        self._sun.set_diffuse([0.15, 0.15, 0.15])
        self._sun.set_ambient([0.05, 0.05, 0.05])

        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "directional_light_component", "test_dir_light")
        yield
        cleanup_object(engine, DYNAMIC_GO)
        self._sun.set_diffuse(self._sun_diffuse)
        self._sun.set_ambient(self._sun_ambient)

    def test_ambient_roundtrip(self, engine, slow):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_ambient([0.3, 0.2, 0.1])
        slow("ambient set")
        _assert_vec3_close(dlc.get_ambient(), [0.3, 0.2, 0.1], label="model ambient")
        entry = _find_directional_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["ambient"], [0.3, 0.2, 0.1], tol=RENDER_TOL, label="render ambient")

    def test_diffuse_roundtrip(self, engine, slow):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_diffuse([1.0, 0.5, 0.0])
        slow("diffuse set")
        _assert_vec3_close(dlc.get_diffuse(), [1.0, 0.5, 0.0], label="model diffuse")
        entry = _find_directional_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["diffuse"], [1.0, 0.5, 0.0], tol=RENDER_TOL, label="render diffuse")

    def test_specular_roundtrip(self, engine, slow):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_specular([0.8, 0.8, 0.8])
        slow("specular set")
        _assert_vec3_close(dlc.get_specular(), [0.8, 0.8, 0.8], label="model specular")
        entry = _find_directional_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["specular"], [0.8, 0.8, 0.8], tol=RENDER_TOL, label="render specular")

    def test_direction_roundtrip(self, engine, slow):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_direction([-1.0, -1.0, 0.0])
        slow("direction set")
        _assert_vec3_close(dlc.get_direction(), [-1.0, -1.0, 0.0], label="model direction")
        entry = _find_directional_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["direction"], [-1.0, -1.0, 0.0], tol=RENDER_TOL, label="render direction")

    def test_selected_roundtrip(self, engine, slow):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_selected(True)
        slow("selected=True")
        assert dlc.get_selected() is True
        dlc.set_selected(False)
        slow("selected=False")
        assert dlc.get_selected() is False

    def test_does_not_corrupt_scene(self, engine, slow):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_diffuse([0.0, 1.0, 0.0])
        slow("diffuse -> green")
        _assert_scene_not_corrupted(engine)


# ---------------------------------------------------------------------------
# Existing directional light (sun) — level-baked, most visible in slow mode
# ---------------------------------------------------------------------------

class TestDirectionalLightRender:

    def test_sun_properties_readable(self, engine, slow):
        dlc = directional_light_component(engine, SUN_COMPONENT_ID)
        ambient = dlc.get_ambient()
        assert len(ambient) == 3
        diffuse = dlc.get_diffuse()
        assert len(diffuse) == 3
        slow("sun read")

    def test_sun_modification_propagates_to_render(self, engine, slow):
        dlc = directional_light_component(engine, SUN_COMPONENT_ID)
        original = dlc.get_diffuse()

        dlc.set_diffuse([1.0, 0.0, 0.0])
        slow("sun -> RED")

        entry = _find_directional_light(engine, SUN_COMPONENT_ID)
        assert entry is not None, f"Sun light {SUN_COMPONENT_ID} not in render.lights.data"
        _assert_vec3_close(entry["diffuse"], [1.0, 0.0, 0.0], tol=RENDER_TOL, label="sun render diffuse")

        dlc.set_diffuse([0.0, 1.0, 0.0])
        slow("sun -> GREEN")

        dlc.set_diffuse([0.0, 0.0, 1.0])
        slow("sun -> BLUE")

        _assert_scene_not_corrupted(engine)

        dlc.set_diffuse(original)
        slow("sun restored")


# ---------------------------------------------------------------------------
# Point light — positioned above hero_cube
# ---------------------------------------------------------------------------

class TestPointLight:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "point_light_component", "test_pt_light")
        _position_light(engine, DYNAMIC_GO)
        plc = point_light_component(engine, self.comp_id)
        plc.set_radius(50.0)
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_ambient_roundtrip(self, engine, slow):
        plc = point_light_component(engine, self.comp_id)
        plc.set_ambient([0.1, 0.2, 0.3])
        slow("ambient set")
        _assert_vec3_close(plc.get_ambient(), [0.1, 0.2, 0.3], label="model ambient")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["ambient"], [0.1, 0.2, 0.3], tol=RENDER_TOL, label="render ambient")

    def test_diffuse_roundtrip(self, engine, slow):
        plc = point_light_component(engine, self.comp_id)
        plc.set_diffuse([0.9, 0.8, 0.7])
        slow("diffuse set")
        _assert_vec3_close(plc.get_diffuse(), [0.9, 0.8, 0.7], label="model diffuse")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["diffuse"], [0.9, 0.8, 0.7], tol=RENDER_TOL, label="render diffuse")

    def test_specular_roundtrip(self, engine, slow):
        plc = point_light_component(engine, self.comp_id)
        plc.set_specular([0.5, 0.5, 0.5])
        slow("specular set")
        _assert_vec3_close(plc.get_specular(), [0.5, 0.5, 0.5], label="model specular")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["specular"], [0.5, 0.5, 0.5], tol=RENDER_TOL, label="render specular")

    def test_radius_roundtrip(self, engine, slow):
        plc = point_light_component(engine, self.comp_id)
        plc.set_radius(15.0)
        slow("radius set")
        assert abs(plc.get_radius() - 15.0) < TOL
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        assert abs(entry["radius"] - 15.0) < RENDER_TOL, (
            f"Render radius={entry['radius']}, expected ~15.0"
        )

    def test_point_light_in_render(self, engine, slow):
        slow("checking render")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None, f"Point light {self.comp_id} not in render.lights.data universal list"
        assert entry["type"] == "point"

    def test_does_not_corrupt_scene(self, engine, slow):
        plc = point_light_component(engine, self.comp_id)
        plc.set_diffuse([0.0, 0.0, 1.0])
        slow("diffuse -> blue")
        _assert_scene_not_corrupted(engine)


# ---------------------------------------------------------------------------
# Spot light — positioned above hero_cube, pointing down
# ---------------------------------------------------------------------------

class TestSpotLight:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "spot_light_component", "test_sp_light")
        _position_light(engine, DYNAMIC_GO)
        slc = spot_light_component(engine, self.comp_id)
        slc.set_direction(SPOT_DIR)
        slc.set_radius(50.0)
        slc.set_cut_off(30.0)
        slc.set_outer_cut_off(45.0)
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_ambient_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_ambient([0.05, 0.05, 0.05])
        slow("ambient set")
        _assert_vec3_close(slc.get_ambient(), [0.05, 0.05, 0.05], label="model ambient")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["ambient"], [0.05, 0.05, 0.05], tol=RENDER_TOL, label="render ambient")

    def test_diffuse_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_diffuse([1.0, 1.0, 0.8])
        slow("diffuse set")
        _assert_vec3_close(slc.get_diffuse(), [1.0, 1.0, 0.8], label="model diffuse")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["diffuse"], [1.0, 1.0, 0.8], tol=RENDER_TOL, label="render diffuse")

    def test_specular_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_specular([0.6, 0.6, 0.6])
        slow("specular set")
        _assert_vec3_close(slc.get_specular(), [0.6, 0.6, 0.6], label="model specular")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["specular"], [0.6, 0.6, 0.6], tol=RENDER_TOL, label="render specular")

    def test_direction_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_direction([0.0, -1.0, 0.0])
        slow("direction set")
        _assert_vec3_close(slc.get_direction(), [0.0, -1.0, 0.0], label="model direction")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        _assert_vec3_close(entry["direction"], [0.0, -1.0, 0.0], tol=RENDER_TOL, label="render direction")

    def test_radius_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_radius(20.0)
        slow("radius set")
        assert abs(slc.get_radius() - 20.0) < TOL
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        assert abs(entry["radius"] - 20.0) < RENDER_TOL, (
            f"Render radius={entry['radius']}, expected ~20.0"
        )

    def test_cut_off_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_cut_off(12.5)
        slow("cut_off set")
        assert abs(slc.get_cut_off() - 12.5) < TOL
        # Render stores cos(radians(degrees))
        expected_cos = math.cos(math.radians(12.5))
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        assert abs(entry["cut_off"] - expected_cos) < RENDER_TOL, (
            f"Render cut_off={entry['cut_off']}, expected cos(12.5°)={expected_cos:.4f}"
        )

    def test_outer_cut_off_roundtrip(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_outer_cut_off(17.5)
        slow("outer_cut_off set")
        assert abs(slc.get_outer_cut_off() - 17.5) < TOL
        expected_cos = math.cos(math.radians(17.5))
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None
        assert abs(entry["outer_cut_off"] - expected_cos) < RENDER_TOL, (
            f"Render outer_cut_off={entry['outer_cut_off']}, expected cos(17.5°)={expected_cos:.4f}"
        )

    def test_spot_light_in_render(self, engine, slow):
        slow("checking render")
        entry = _find_universal_light(engine, self.comp_id)
        assert entry is not None, f"Spot light {self.comp_id} not in render.lights.data universal list"
        assert entry["type"] == "spot"

    def test_does_not_corrupt_scene(self, engine, slow):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_diffuse([0.0, 1.0, 0.0])
        slow("diffuse -> green")
        _assert_scene_not_corrupted(engine)

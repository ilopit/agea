"""Light components — directional, point, spot property coverage + render verification."""
import pytest
from base import directional_light_component, point_light_component, spot_light_component
from .property_helpers import (
    create_test_object, add_component, cleanup_object,
    get_property, set_property, assert_property_roundtrip,
)

DYNAMIC_GO = "test_light_go"


# ---------------------------------------------------------------------------
# Directional light
# ---------------------------------------------------------------------------

class TestDirectionalLight:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "directional_light_component", "test_dir_light")
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_ambient_roundtrip(self, engine):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_ambient([0.3, 0.2, 0.1])
        val = dlc.get_ambient()
        for i, e in enumerate([0.3, 0.2, 0.1]):
            assert abs(val[i] - e) < 0.01

    def test_diffuse_roundtrip(self, engine):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_diffuse([1.0, 0.5, 0.0])
        val = dlc.get_diffuse()
        for i, e in enumerate([1.0, 0.5, 0.0]):
            assert abs(val[i] - e) < 0.01

    def test_specular_roundtrip(self, engine):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_specular([0.8, 0.8, 0.8])
        val = dlc.get_specular()
        for i, e in enumerate([0.8, 0.8, 0.8]):
            assert abs(val[i] - e) < 0.01

    def test_direction_roundtrip(self, engine):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_direction([-1.0, -1.0, 0.0])
        val = dlc.get_direction()
        for i, e in enumerate([-1.0, -1.0, 0.0]):
            assert abs(val[i] - e) < 0.01

    def test_selected_roundtrip(self, engine):
        dlc = directional_light_component(engine, self.comp_id)
        dlc.set_selected(True)
        assert dlc.get_selected() is True
        dlc.set_selected(False)
        assert dlc.get_selected() is False


# ---------------------------------------------------------------------------
# Existing directional light (sun) — render verification
# ---------------------------------------------------------------------------

class TestDirectionalLightRender:

    def test_sun_properties_readable(self, engine):
        dlc = directional_light_component(engine, "sun_lc")
        ambient = dlc.get_ambient()
        assert len(ambient) == 3
        diffuse = dlc.get_diffuse()
        assert len(diffuse) == 3

    def test_sun_modification_propagates_to_render(self, engine):
        dlc = directional_light_component(engine, "sun_lc")
        original = dlc.get_diffuse()

        dlc.set_diffuse([1.0, 0.0, 0.0])
        engine.wait_frame(2)

        lights = engine.call("render.lights.data")
        dir_lights = lights.get("directional", [])
        sun = next((l for l in dir_lights if l["id"] == "sun_lc"), None)
        assert sun is not None
        assert abs(sun["diffuse"][0] - 1.0) < 0.1, (
            f"Render diffuse[0]={sun['diffuse'][0]}, expected ~1.0"
        )

        dlc.set_diffuse(original)
        engine.wait_frame()


# ---------------------------------------------------------------------------
# Point light
# ---------------------------------------------------------------------------

class TestPointLight:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "point_light_component", "test_pt_light")
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_ambient_roundtrip(self, engine):
        plc = point_light_component(engine, self.comp_id)
        plc.set_ambient([0.1, 0.2, 0.3])
        val = plc.get_ambient()
        for i, e in enumerate([0.1, 0.2, 0.3]):
            assert abs(val[i] - e) < 0.01

    def test_diffuse_roundtrip(self, engine):
        plc = point_light_component(engine, self.comp_id)
        plc.set_diffuse([0.9, 0.8, 0.7])
        val = plc.get_diffuse()
        for i, e in enumerate([0.9, 0.8, 0.7]):
            assert abs(val[i] - e) < 0.01

    def test_specular_roundtrip(self, engine):
        plc = point_light_component(engine, self.comp_id)
        plc.set_specular([0.5, 0.5, 0.5])
        val = plc.get_specular()
        for i, e in enumerate([0.5, 0.5, 0.5]):
            assert abs(val[i] - e) < 0.01

    def test_radius_roundtrip(self, engine):
        plc = point_light_component(engine, self.comp_id)
        plc.set_radius(15.0)
        assert abs(plc.get_radius() - 15.0) < 0.01

    def test_point_light_in_render(self, engine):
        engine.wait_frame()
        lights = engine.call("render.lights.data")
        universal = lights.get("universal", [])
        found = any(l["id"] == self.comp_id for l in universal)
        assert found, f"Point light {self.comp_id} not in render.lights.data universal list"


# ---------------------------------------------------------------------------
# Spot light
# ---------------------------------------------------------------------------

class TestSpotLight:

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        create_test_object(engine, DYNAMIC_GO)
        self.comp_id = add_component(engine, DYNAMIC_GO, "spot_light_component", "test_sp_light")
        yield
        cleanup_object(engine, DYNAMIC_GO)

    def test_ambient_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_ambient([0.05, 0.05, 0.05])
        val = slc.get_ambient()
        for i, e in enumerate([0.05, 0.05, 0.05]):
            assert abs(val[i] - e) < 0.01

    def test_diffuse_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_diffuse([1.0, 1.0, 0.8])
        val = slc.get_diffuse()
        for i, e in enumerate([1.0, 1.0, 0.8]):
            assert abs(val[i] - e) < 0.01

    def test_specular_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_specular([0.6, 0.6, 0.6])
        val = slc.get_specular()
        for i, e in enumerate([0.6, 0.6, 0.6]):
            assert abs(val[i] - e) < 0.01

    def test_direction_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_direction([0.0, -1.0, 0.0])
        val = slc.get_direction()
        for i, e in enumerate([0.0, -1.0, 0.0]):
            assert abs(val[i] - e) < 0.01

    def test_radius_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_radius(20.0)
        assert abs(slc.get_radius() - 20.0) < 0.01

    def test_cut_off_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_cut_off(12.5)
        assert abs(slc.get_cut_off() - 12.5) < 0.01

    def test_outer_cut_off_roundtrip(self, engine):
        slc = spot_light_component(engine, self.comp_id)
        slc.set_outer_cut_off(17.5)
        assert abs(slc.get_outer_cut_off() - 17.5) < 0.01

    def test_spot_light_in_render(self, engine):
        engine.wait_frame()
        lights = engine.call("render.lights.data")
        universal = lights.get("universal", [])
        found = any(l["id"] == self.comp_id for l in universal)
        assert found, f"Spot light {self.comp_id} not in render.lights.data universal list"

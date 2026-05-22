"""Render state queries — config, camera, stats."""


# ---------------------------------------------------------------------------
# Render config
# ---------------------------------------------------------------------------

class TestRenderConfig:

    def test_get_returns_valid_structure(self, engine):
        config = engine.call("render.config.get")
        assert isinstance(config, dict)
        assert "shadows" in config or "render_scale" in config or len(config) > 0

    def test_set_shadows_enabled(self, engine):
        config = engine.call("render.config.get")
        if "shadows" not in config:
            import pytest
            pytest.skip("shadows not in config")
        original_enabled = config["shadows"].get("enabled", True)

        engine.call("render.config.set", {"shadows": {"enabled": not original_enabled}})
        engine.wait_frame()
        updated = engine.call("render.config.get")
        assert updated["shadows"]["enabled"] == (not original_enabled)

        engine.call("render.config.set", {"shadows": {"enabled": original_enabled}})
        engine.wait_frame()

    def test_set_render_scale_divisor(self, engine):
        config = engine.call("render.config.get")
        if "render_scale" not in config:
            import pytest
            pytest.skip("render_scale not in config")
        original = config["render_scale"]

        engine.call("render.config.set", {"render_scale": {"divisor": 2}})
        engine.wait_frame()
        updated = engine.call("render.config.get")
        assert updated["render_scale"]["divisor"] == 2

        engine.call("render.config.set", {"render_scale": original})
        engine.wait_frame()

    def test_set_debug_show_grid(self, engine):
        config = engine.call("render.config.get")
        if "debug" not in config:
            import pytest
            pytest.skip("debug not in config")
        original = config["debug"].get("show_grid", False)

        engine.call("render.config.set", {"debug": {"show_grid": not original}})
        engine.wait_frame()
        updated = engine.call("render.config.get")
        assert updated["debug"]["show_grid"] == (not original)

        engine.call("render.config.set", {"debug": {"show_grid": original}})
        engine.wait_frame()

    def test_partial_update_preserves_other_fields(self, engine):
        config_before = engine.call("render.config.get")
        if "lighting" not in config_before:
            import pytest
            pytest.skip("lighting not in config")
        original_lighting = config_before["lighting"]

        engine.call("render.config.set", {"lighting": original_lighting})
        engine.wait_frame()
        config_after = engine.call("render.config.get")

        for key in ("shadows", "render_scale", "debug"):
            if key in config_before:
                assert config_after.get(key) == config_before.get(key), (
                    f"Config key '{key}' changed unexpectedly"
                )


# ---------------------------------------------------------------------------
# Render camera
# ---------------------------------------------------------------------------

class TestRenderCamera:

    def test_camera_data_returns_matrices(self, engine):
        cam = engine.call("render.camera.data")
        assert "position" in cam
        assert "view" in cam or "view_matrix" in cam
        assert "projection" in cam or "projection_matrix" in cam

    def test_camera_position_is_vec3(self, engine):
        cam = engine.call("render.camera.data")
        pos = cam["position"]
        assert len(pos) == 3

    def test_camera_after_editor_set(self, engine):
        engine.call("editor.camera.set", {
            "position": [10.0, 5.0, 0.0], "pitch": -30.0, "yaw": 45.0,
        })
        engine.wait_frame()
        cam = engine.call("render.camera.data")
        pos = cam["position"]
        assert abs(pos[0] - 10.0) < 1.0, f"Camera X={pos[0]}, expected ~10"


# ---------------------------------------------------------------------------
# Render stats
# ---------------------------------------------------------------------------

class TestRenderStats:

    def test_stats_returns_counts(self, engine):
        stats = engine.call("render.stats")
        assert "object_count" in stats
        assert stats["object_count"] >= 0

    def test_stats_object_count_matches_list(self, engine):
        stats = engine.call("render.stats")
        objs = engine.call("render.object.list")
        listed = len(objs.get("objects", []))
        assert stats["object_count"] == listed, (
            f"stats.object_count={stats['object_count']} != listed={listed}"
        )

    def test_stats_has_light_count(self, engine):
        stats = engine.call("render.stats")
        assert "light_count" in stats or "directional_light_count" in stats

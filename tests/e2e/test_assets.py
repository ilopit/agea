"""Asset properties — texture metadata, sampler properties, object identity.

Covers the reflected property surface of non-component asset types:
- texture: width/height read-back via model.list(kind=texture) + property proxy
- sampler: filter, address mode, and anisotropy round-trip via property proxy
- smart_object: model.list returns valid IDs, property.get returns owners

These are package-level assets (not level instances), so tests query loaded
packages rather than creating objects dynamically.
"""
import pytest
from root import texture, sampler


# ---------------------------------------------------------------------------
# Texture metadata
# ---------------------------------------------------------------------------

class TestTextureMetadata:
    """Verify texture assets expose readable width/height through the property system.

    Textures are loaded from .apkg packages at startup. We don't set dimensions
    (they come from the image file), but we verify the RPC can read them — this
    exercises the texture property pipeline end-to-end.
    """

    def test_texture_list_nonempty(self, engine):
        """model.list with kind=texture returns at least one texture from loaded packages."""
        result = engine.call("model.list", {"source": "all", "kind": "texture"})
        tex_ids = [t["id"] for t in result.get("items", [])]
        assert len(tex_ids) > 0

    def test_texture_width_and_height(self, engine):
        """First texture has positive integer width and height."""
        result = engine.call("model.list", {"source": "all", "kind": "texture"})
        tex_list = result.get("items", [])
        assert len(tex_list) > 0
        tex_id = tex_list[0]["id"]
        t = texture(engine, tex_id)
        w = t.get_width()
        h = t.get_height()
        assert isinstance(w, int) and w > 0, f"texture width={w}"
        assert isinstance(h, int) and h > 0, f"texture height={h}"


# ---------------------------------------------------------------------------
# Sampler properties
# ---------------------------------------------------------------------------

def _find_sampler(engine):
    items = engine.call("model.list", {"source": "all", "kind": "sampler"})
    for item in items.get("items", []):
        return item["id"]
    return None


class TestSamplerProperties:
    """Verify sampler asset properties are readable through the property proxy.

    Samplers are class objects (readonly) loaded from packages — no instance
    copies exist, so writes are blocked by design. These tests verify that the
    reflection pipeline correctly exposes sampler properties for reading.
    """

    @pytest.fixture(autouse=True)
    def _setup(self, engine):
        self.sampler_id = _find_sampler(engine)
        if self.sampler_id is None:
            pytest.skip("No sampler found in loaded packages")

    def test_min_filter_readable(self, engine):
        s = sampler(engine, self.sampler_id)
        val = s.get_min_filter()
        assert isinstance(val, int) and 0 <= val <= 1

    def test_mag_filter_readable(self, engine):
        s = sampler(engine, self.sampler_id)
        val = s.get_mag_filter()
        assert isinstance(val, int) and 0 <= val <= 1

    def test_address_u_readable(self, engine):
        s = sampler(engine, self.sampler_id)
        val = s.get_address_u()
        assert isinstance(val, int) and 0 <= val <= 3

    def test_address_v_readable(self, engine):
        s = sampler(engine, self.sampler_id)
        val = s.get_address_v()
        assert isinstance(val, int) and 0 <= val <= 3

    def test_anisotropy_readable(self, engine):
        s = sampler(engine, self.sampler_id)
        val = s.get_anisotropy()
        assert isinstance(val, bool)


# ---------------------------------------------------------------------------
# Object identity
# ---------------------------------------------------------------------------

class TestObjectIdentity:
    """Verify the model layer's object listing and property system basics.

    These are sanity checks that model.list returns well-formed items with
    string IDs, and that model.object.property.get returns at least one
    property owner per object. Failures here indicate a broken reflection
    or serialization pipeline.
    """

    def test_model_list_objects_have_ids(self, engine):
        """Every item in model.list has a non-empty string 'id' field."""
        items = engine.call("model.list")["items"]
        assert len(items) > 0
        for item in items:
            assert "id" in item
            assert isinstance(item["id"], str) and len(item["id"]) > 0

    def test_object_properties_include_owner(self, engine):
        """model.object.property.get returns at least one owner with categories."""
        items = engine.call("model.list")["items"]
        obj_id = items[0]["id"]
        props = engine.call("model.object.property.get", {"id": obj_id})
        owners = props.get("owners", [])
        assert len(owners) > 0, f"No property owners for {obj_id}"

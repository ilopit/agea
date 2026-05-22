"""Shared utilities for property round-trip testing and dynamic object creation."""


def get_property(engine, obj_id, name):
    props = engine.call("model.object.property.get", {"id": obj_id})
    for owner in props.get("owners", []):
        if owner["id"] != obj_id:
            continue
        for cat in owner.get("categories", []):
            for f in cat.get("fields", []):
                if f["name"] == name:
                    return f.get("value")
    return None


def set_property(engine, obj_id, name, value):
    engine.call("model.object.property.set", {
        "owner_id": obj_id, "name": name, "value": value,
    })
    engine.wait_frame()


def assert_property_roundtrip(engine, obj_id, name, value, tol=None):
    set_property(engine, obj_id, name, value)
    actual = get_property(engine, obj_id, name)
    if tol is not None and isinstance(value, (list, tuple)):
        for i, (a, e) in enumerate(zip(actual, value)):
            assert abs(a - e) < tol, (
                f"{name}[{i}] on {obj_id}: got {a}, expected {e} (tol={tol})"
            )
    elif tol is not None and isinstance(value, (int, float)):
        assert abs(actual - value) < tol, (
            f"{name} on {obj_id}: got {actual}, expected {value} (tol={tol})"
        )
    else:
        assert actual == value, (
            f"{name} on {obj_id}: got {actual}, expected {value}"
        )


def create_test_object(engine, name):
    engine.call("model.scene.create", {"name": name})
    return name


def add_component(engine, obj_id, type_id, name, parent_id=None, props=None):
    params = {"object_id": obj_id, "type_id": type_id, "name": name}
    if parent_id:
        params["parent_id"] = parent_id
    if props:
        params["properties"] = props
    r = engine.call("model.component.add", params)
    engine.wait_frame()
    return r["id"]


def cleanup_object(engine, obj_id):
    try:
        engine.call("model.scene.delete", {"id": obj_id})
        engine.wait_frame()
    except RuntimeError:
        pass


def get_material_fields(engine, mat_id):
    r = engine.call("model.material.get", {"id": mat_id})
    cats = r["material"]["categories"]
    props_cat = next(c for c in cats if c["name"] == "Properties")
    return {f["name"]: f["value"] for f in props_cat["fields"] if "value" in f}


def get_texture_slots(engine, mat_id):
    fields = get_material_fields(engine, mat_id)
    return {
        name: val["texture"]
        for name, val in fields.items()
        if isinstance(val, dict) and "texture" in val
    }


def assert_material_field_roundtrip(engine, mat_id, field, value, save=True):
    original = get_material_fields(engine, mat_id).get(field)
    engine.call("model.material.edit", {"id": mat_id})
    engine.call("model.material.setField", {
        "id": mat_id, "field": field, "value": value,
    })
    if save:
        engine.call("model.material.save", {"id": mat_id})
        engine.wait_frame()
        fields = get_material_fields(engine, mat_id)
        if isinstance(value, (list, tuple)):
            for i, (a, e) in enumerate(zip(fields[field], value)):
                assert abs(a - e) < 0.01, (
                    f"{field}[{i}] on {mat_id}: got {a}, expected {e}"
                )
        elif isinstance(value, float):
            assert abs(fields[field] - value) < 0.01, (
                f"{field} on {mat_id}: got {fields[field]}, expected {value}"
            )
        else:
            assert fields[field] == value, (
                f"{field} on {mat_id}: got {fields[field]}, expected {value}"
            )
        engine.call("model.material.edit", {"id": mat_id})
        engine.call("model.material.setField", {
            "id": mat_id, "field": field, "value": original,
        })
        engine.call("model.material.save", {"id": mat_id})
        engine.wait_frame()
    else:
        engine.call("model.material.discard", {"id": mat_id})

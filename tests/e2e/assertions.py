"""
Assertion helpers for verifying engine render state.
"""

UINT32_MAX = 0xFFFFFFFF
ERROR_MATERIAL = "se_error"


def assert_valid_material(render_obj: dict, obj_id: str = "") -> None:
    ctx = f" on '{obj_id}'" if obj_id else ""
    mat = render_obj.get("material")
    assert mat is not None, f"No material data{ctx}"
    assert mat["id"] != ERROR_MATERIAL, (
        f"Material is '{ERROR_MATERIAL}'{ctx} — shader/material load failure (pink bug)"
    )


def assert_valid_textures(render_obj: dict, obj_id: str = "") -> None:
    ctx = f" on '{obj_id}'" if obj_id else ""
    mat = render_obj.get("material")
    assert mat is not None, f"No material data{ctx}"
    tex = mat.get("texture_indices", [])
    if tex:
        assert not all(idx == UINT32_MAX for idx in tex), (
            f"All texture_indices are UINT32_MAX{ctx} — textures not bound"
        )


def assert_not_pink_bug(render_obj: dict, obj_id: str = "") -> None:
    assert_valid_material(render_obj, obj_id)
    assert_valid_textures(render_obj, obj_id)


def assert_render_object_exists(objects_result: dict, obj_id: str) -> dict:
    objs = objects_result.get("objects", [])
    for obj in objs:
        if obj.get("id") == obj_id:
            return obj
    found_ids = [o.get("id", "?") for o in objs]
    raise AssertionError(f"Render object '{obj_id}' not found. Present: {found_ids}")


def assert_render_object_gone(objects_result: dict, obj_id: str) -> None:
    objs = objects_result.get("objects", [])
    found = [o for o in objs if o.get("id") == obj_id]
    assert not found, f"Render object '{obj_id}' still present but expected deleted"


def assert_position_close(
    actual: list, expected: list, tolerance: float = 0.01, label: str = ""
) -> None:
    ctx = f" ({label})" if label else ""
    assert len(actual) == 3 and len(expected) == 3, f"Position must be [x,y,z]{ctx}"
    for i, (a, e) in enumerate(zip(actual, expected)):
        assert abs(a - e) < tolerance, (
            f"Position[{i}] differs{ctx}: {a} vs expected {e} (tol={tolerance})"
        )


def assert_mesh_matches(
    render_obj: dict, expected_mesh_id: str, obj_id: str = ""
) -> None:
    ctx = f" on '{obj_id}'" if obj_id else ""
    mesh = render_obj.get("mesh")
    assert mesh is not None, f"No mesh data{ctx}"
    assert mesh["id"] == expected_mesh_id, (
        f"Mesh mismatch{ctx}: got '{mesh['id']}', expected '{expected_mesh_id}'"
    )


def assert_material_id_matches(
    render_obj: dict, expected_mat_id: str, obj_id: str = ""
) -> None:
    ctx = f" on '{obj_id}'" if obj_id else ""
    mat = render_obj.get("material")
    assert mat is not None, f"No material data{ctx}"
    assert mat["id"] == expected_mat_id, (
        f"Material mismatch{ctx}: got '{mat['id']}', expected '{expected_mat_id}'"
    )

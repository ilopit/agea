# Packages — Reflection & Serialization

## KRG_ar_property annotations

Properties are declared with `KRG_ar_property(...)` and processed by `tools/argen.py` to generate reflection metadata. Key annotations:

| Annotation | Values | Effect |
|---|---|---|
| `category` | string | Groups property in editor UI (`m_editor_properties` map key). `"Meta"` is internal-only, skipped by inspector. |
| `serializable` | `true`/`false` | Controls whether the property enters `m_serialization_properties`. Only serializable properties participate in `object_save` / `diff_object_properties` / `object_load`. |
| `access` | `all`, `read_only`, `cpp_only`, `no` | `all` = editor read+write. `read_only` = editor can see, not edit. `no` = no editor widget generated. `cpp_only` = C++ getter only. |
| `gpu_data` | e.g. `MaterialData` | argen generates a GPU-side struct (e.g. `simple_texture_material__gpu`) with this property. |
| `default` | `true` | Property has a C++ default value; argen uses it for the CDO. |
| `invalidates` | `render` | Changing this property should trigger GPU re-upload. |
| `property_*_handler` | function name | Override default save/load/compare/copy/instantiate handlers (used for complex types like `texture_slot`). |

## Serialization vs editor visibility

A property can be **visible in the editor** (`m_editor_properties`) but **not serializable** (`m_serialization_properties`). This happens when `serializable=false`.

Consequences:
- `object_constructor::diff_object_properties()` iterates `m_serialization_properties` — non-serializable properties are invisible to it.
- `object_constructor::object_save()` calls `diff_object_properties` then `save_handler` per diff entry — non-serializable properties are never saved to `.aobj`.
- `property::json_get` / `property::json_set` still work on non-serializable properties (they operate on live memory), so the editor inspector can read/write them at runtime. But changes are lost on reload.

The `property` struct carries `bool serializable` (property.h:100) — use this to determine persistence capability at runtime.

## gpu_data properties

Properties with `gpu_data=MaterialData` + `serializable=false` exist solely for argen to generate GPU struct layouts. Their C++ default values define the GPU data, but they cannot be overridden per-material-instance via `.aobj` files.

Example: `simple_texture_material` has `m_diffuse`, `m_ambient`, `m_specular`, `m_shininess` all marked `serializable=false, gpu_data=MaterialData`. These generate `simple_texture_material__gpu` but cannot be persisted per-derived-material.

Contrast: `solid_color_material`, `toon_material`, `pbr_material` have their value properties marked `serializable=true` — those CAN be overridden and saved per-material.

## Property collections in reflection_type

- `m_editor_properties`: `map<string, vector<property>>` keyed by category. All properties with a category, including non-serializable ones.
- `m_serialization_properties`: `vector<property>`. Only properties with `serializable=true`. Used by object_constructor for save/load/diff/compare.

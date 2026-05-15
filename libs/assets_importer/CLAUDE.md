# Assets Importer

Converts external 3D assets into Kryga's native binary formats.

## Pipeline
**Mesh:** OBJ → tiny_obj_loader → `gpu::vertex_data` (pos, normal, UV0, color, UV2) → xatlas UV2 generation → serialize to `.aobj` + `.avrt` + `.aind`

**Texture:** image file → stb_image decode → RGBA8 buffer → serialize to `.aobj` + `.atbc`

## Entry points
- `convert_3do_to_amsh()` — mesh import
- `convert_imager_to_atxt()` — texture import

## UV2 generation (xatlas)
Auto-generates lightmap UVs during mesh import. xatlas accesses vertex data via field pointers + stride assuming contiguous `gpu::vertex_data` layout.

## Gotchas
- xatlas splits vertices at UV seams — output vertex/index count can exceed input
- UV V-flip during OBJ load (`1 - uy`) to match engine texture convention
- xatlas failure logs warning but continues with invalid UV2 silently — no error recovery
- Both converters create dummy `core::package` instances for serialization context

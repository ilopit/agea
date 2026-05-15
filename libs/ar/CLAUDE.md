# AR — Reflection / Annotation System

Macro-based C++ reflection. Macros are parsed by `tools/argen.py` which generates runtime reflection code into `build/kryga_generated/`.

## Macros (defined in ar_defines.h as empty stubs for the parser)
- `KRG_ar_class()` / `KRG_ar_struct()` — mark types for reflection
- `KRG_ar_property(...)` — annotate properties (category, gpu_data, serializable, access level)
- `KRG_ar_function` / `KRG_ar_ctor` — reflect functions and constructors
- `KRG_ar_external_type()` — register non-KRYGA types
- `KRG_ar_model_overrides()` / `KRG_ar_render_overrides()` — custom code injection points
- `KRG_ar_package` / `KRG_ar_type` — package and type metadata

## Code generation pipeline
1. argen.py reads C++ headers for `KRG_ar_*` macros (`tools/arapi/parser.py`)
2. Types are topologically sorted by inheritance (`order_types_by_parent()` via DFS)
3. Writer generates `.ar.cpp` / `.ar.h` per class (`tools/arapi/writer.py`)
4. Generated files are included by the actual class definitions
5. Type IDs generated from namespace tokens, filtering out `std`/`kryga`/`root`

## GPU data marshaling
Properties marked `gpu_data` get GLSL-compatible struct layouts with explicit alignment/packing. Textures become bindless texture indices.

## Gotchas
- `PKG_KEY_DEPENDENCIES` uses "dependancies" (typo is intentional — backward compat with existing data)
- Never read `*.ar.cpp` files without permission (project rule)
- Property handlers support custom serialization, comparison, and copy — not just data

---
name: mcp-annotate
description: Guide for writing KRG_ar_class/KRG_ar_property/KRG_ar_function mcp_schema and mcp_hint annotations. Use when adding or reviewing MCP metadata on reflected types, properties, or functions.
argument-hint: "[type|property|function] [file_path]"
allowed-tools: Read, Edit, Glob, Grep
---

# MCP Annotation Guide

## Argen tokenizer constraints

- **No commas** inside `mcp_hint` — the tokenizer splits on `,` before parsing key=value
- **No parentheses** inside `mcp_hint` — `)` terminates the macro
- Use `/` or `;` instead of commas, `[]` or `:` instead of `()`
- Hints on properties use double quotes: `mcp_hint="text"`
- Hints on functions use single quotes: `mcp_hint = 'text'`

## mcp_schema

Describes the value format. Set on `KRG_ar_class`, `KRG_ar_struct`, or `KRG_ar_property`. Inherited by child types from parent.

### Placement

- **On a type** (`KRG_ar_class` / `KRG_ar_struct`): applies to all instances of this type. Properties of this type inherit the schema via `rtype->mcp_schema`.
- **On a property** (`KRG_ar_property`): overrides the type-level schema for that specific field.

### Values

Schema is a free-form string — not an enum. Convention is `category` or `category:subtype`. Existing examples:

| Pattern | Examples | Purpose |
|---|---|---|
| `string` | plain text | |
| `string:<subtype>` | `string:object_id` / `string:asset_id` / `string:id` / `string:base64` | qualified string semantics |
| `number` | float / double | |
| `integer` | int / uint types | |
| `boolean` | bool | |
| `array:<element>` | `array:number` / `array:string` | typed collection |
| `array:<element>:<size>` | `array:number:3` / `array:number:4` | fixed-size typed collection (vec3 = 3 / vec4 = 4) |

New subtypes can be added freely — just keep the `category:subtype` convention and document in the type's `mcp_hint` what it means.

### Hint inclusion rule

The three-tier MCP system uses schema to decide when to include hints in responses:
- `number` / `boolean` / `array:*` — hints skipped (self-explanatory values)
- `integer` / `string` / `string:*` — hints included (values need context)

### Example

```cpp
KRG_ar_class("architype=game_object",
              mcp_schema          = "string:object_id",
              mcp_hint             = "Scene entity that owns components");

KRG_ar_struct(mcp_schema          = "array:number",
              mcp_hint            = "3D vector [x y z]");
```

## mcp_hint — on types, properties, and functions

Short description helping an AI model understand purpose and usage. Not a docstring — focus on what an API consumer needs to know.

### Type hints

One sentence: what it is + what makes it distinct from siblings.

```cpp
KRG_ar_class(mcp_hint = "Component with spatial transform: position / rotation / scale and visibility layer flags");
```

### Property hints

What the value controls and any non-obvious constraints. Skip hints for self-explanatory properties (e.g., `name` on a named object).

```cpp
KRG_ar_property("category=Rendering",
                "serializable=true",
                "access=all",
                mcp_hint="Diffuse color multiplier [r g b] in linear space");
```

### Function hints

Pattern: `<verb> <what> [format]`

Array/vector components use `[component component ...]` with spaces — never commas:

| Type | Pattern |
|---|---|
| vec2 | `[x y]` |
| vec3 position/scale | `[x y z]` |
| vec3 color | `[r g b]` |
| vec4 color | `[r g b a]` |
| quaternion | `[x y z w]` |
| matrix row | `[m00 m01 m02 ...]` |

Use semantic names that match the context — `[r g b]` for colors, `[x y z]` for spatial.

```cpp
KRG_ar_function("category=world",
                 mcp_hint = 'Returns world position [x y z]');

KRG_ar_function("category=world",
                 mcp_hint = 'Sets euler rotation in degrees [x y z]');

KRG_ar_function("category=world",
                 mcp_hint = 'Adds relative offset [x y z] to current position');
```

## What gets exposed via MCP

The three-tier read system uses these annotations:

1. **kryga_model_get_all** — returns all properties with values. Includes `hint` when schema is `integer` / `string` / `string:*`
2. **kryga_model_get_property** — single property with value + selective hint (same schema rule)
3. **kryga_model_get_type_meta** — full type metadata: all properties with hints, all functions with hints, schema, parent type

## Checklist

When annotating a file:

1. Set `mcp_schema` on the top-level class if it's addressable (object_id) or an asset (asset_id)
2. Add `mcp_hint` to the class describing what it is
3. Add `mcp_hint` to non-obvious properties — skip trivial ones
4. Add `KRG_ar_function` + `mcp_hint` to public methods useful for MCP consumers
5. Use consistent coordinate format: `[x y z]` not `[x/y/z]` or `(x, y, z)`
6. Verify no commas or parentheses in any hint text
7. Rebuild: `cmake --preset host && tools/build.sh kryga_editor`

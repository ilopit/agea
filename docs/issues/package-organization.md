# Package organization

**Type:** refactoring / tech debt
**Scope:** `packages/` layout, type-loading pipeline

## Issue

Current `packages/` directory conflates two different kinds of package content:

- **Asset packages** — runtime data (meshes, textures, levels, materials).
- **Module / type packages** — C++ class modules and their reflection metadata.

Mixing them makes load ordering awkward, reuse harder, and obscures which packages are hot-reloadable content vs. code-tied definitions.

## Proposed fix

Split into two kinds of package with distinct top-level directories or naming convention:

- Asset packages — pure data, versioned, reloadable.
- Module / type packages — tied to compiled code, contain reflection + type definitions.

This affects:

- `packages/` layout.
- Package manager / loader in `libs/core`.
- Type-loading stages (see `README.md` — "Stages of type loading").

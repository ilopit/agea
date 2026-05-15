# Known Issues & Technical Debt

Per-subsystem tracking of known bugs, limitations, and deferred work in the kryga engine.

Not design scope — the design docs live in [`../plans/`](../plans/). Items here may become design work if they block a milestone.

## Index

| File | Subsystem | Summary |
|------|-----------|---------|
| [`meshes.md`](meshes.md) | Mesh import & vertex layout | UV2 overhead, OBJ importer bugs |
| [`shadows.md`](shadows.md) | Shadow rendering | Cascade coverage, frustum culling, DPSM, bias, atlas |
| [`validation.md`](validation.md) | Vulkan validation & debug | Layer state, deferred validation features, naming gaps |
| [`editor-ux.md`](editor-ux.md) | Editor / ImGui | Full editor UX audit: broken features, workflow gaps, camera, interaction, visual |
| [`editor-ui-state.md`](editor-ui-state.md) | Editor / ImGui | `CollapsingHeader` state not persisted (folded into editor-ux.md #16) |
| [`package-organization.md`](package-organization.md) | Core / packages | Split asset packages from module/type packages |
| [`reflection.md`](reflection.md) | Core / reflection | Transient object flag, serializable vs editable gap |

## Conventions

- One file per subsystem. Items numbered within a file.
- Severity tag in brackets where useful: `[major]`, `[medium]`, `[low]`, `[minor]`. Secondary tag optional (e.g. `[medium, correctness]`).
- File/line citations preferred where they help.
- Entries should describe the issue, its effect, and at least one proposed fix or alternative — not just a complaint.
- Closed items: remove from the file. The git log is the audit trail.

# Editor UX — Known Issues & Improvements

Comprehensive audit of the ImGui-based editor experience. Covers broken functionality, workflow gaps, interaction model, and visual polish.

Supersedes `editor-ui-state.md` (CollapsingHeader issue folded in as item 16).

---

## Broken / Non-Functional

### ~~1. Script editor~~ — removed

Script editor dropped entirely. Was dead code (never registered in `m_windows`). `textedit_unofficial` dependency unlinked.

### ~~2. Material preview~~ — implemented

CPU-rendered sphere preview via `material.preview` RPC endpoint. Engine extracts Phong colors (ambient/diffuse/specular/shininess) from material GPU data, ray-traces a sphere, returns base64 PNG. VS Code inspector shows preview for material-type components. No texture sampling yet (v2).

### ~~3. Property drawer system disabled~~ — resolved

Old `property_drawers.cpp` deleted. Property editing now via VS Code inspector (`properties.get`/`properties.set` RPC).

### ~~4. Performance counters divide by zero~~ — fixed

Guarded `culled_draws_avg / all_draws_avg` with `all_draws_avg > 0` check.

### 5. Performance counters hidden during play mode [low]

Play mode intentionally hides all editor windows except perf counters. Perf counters ARE visible in edit mode (via `m_windows` loop). Original audit was incorrect — counters work in both modes. Remaining issue: perf counters could also be exposed via RPC for VS Code overlay.

---

## Workflow Gaps

### 6. No undo/redo [major]

No history tracking of any kind. Gizmo drags, property changes, object creation — all permanent. Ctrl+Z does nothing. This is the single largest editor productivity gap.

**Approach:** Command pattern with action stack. Each undoable operation stores before/after state (or a reversible delta). Gizmo manipulations coalesce into a single undo entry on mouse-up.

### 7. No object deletion from editor [major]

Delete key not bound to any action. Objects can only be deleted via RPC (`scene.delete`) or code. No confirmation dialog either.

**Fix:** Bind Delete key, confirm on multi-component objects, push through undo system (item 6).

### ~~8. Properties are read-only in inspector~~ — resolved

Old `object_editor` deleted. VS Code inspector provides read/write property editing via `properties.get`/`properties.set` RPC.

### 9. No copy/paste/duplicate objects [medium]

No Ctrl+C/V/D bindings. Duplication only via RPC (`scene.duplicate`). No clipboard for cross-level object transfer.

### 10. No multi-selection [medium]

`editor.h:31-33` — Single `utils::id m_selected`. Comment says "multi-select can extend this later." No Ctrl+Click, Shift+Click, or box select. Blocks batch operations (delete multiple, move group, bulk property edit).

### 11. No scene search or filter [medium]

Level editor tree and package editor tree have no search box. Large scenes require expanding every node manually to find an object.

**Fix:** `ImGui::InputText` filter at top of each tree panel. Match against object name/type.

### 12. No drag-drop in hierarchy [low-medium]

Can't reparent objects by dragging in the Level Editor tree. Can't drag assets from Package Editor to viewport.

### 13. No action cancellation [medium]

`action_progress_window.cpp` — Long-running actions (lightmap bake, asset conversion) cannot be interrupted. No cancel button. User must wait or kill the process.

**Fix:** Add cancellation token to action system. Baker and converter check token between phases.

---

## Camera & Navigation

### 14. FPS-only camera model [major]

`editor.cpp:273-356` — Only WASD + right-click mouselook. Sensitivity hardcoded at 500. Missing the standard 3D editor camera toolkit:

| Feature | Status |
|---------|--------|
| Orbit (MMB drag) | Missing |
| Pan (Shift+MMB or MMB) | Missing |
| Scroll-wheel zoom / dolly | Missing |
| Focus on selection (F key) | Missing |
| Frame all (Home or Shift+F) | Missing |
| Camera bookmarks | Missing |

**Impact:** Navigating around objects requires walking with WASD. No way to quickly inspect a selected object from multiple angles.

**Approach:** Orbit/pan/zoom mode as default, with existing FPS mode as alt-tab fallback (hold right-click). Focus-on-selection computes bounding sphere and sets orbit pivot + distance.

---

## Interaction Polish

### 15. No keyboard shortcut discovery [low-medium]

Shortcuts exist (1/2/3 for gizmo, F5 for play, R for reload, Esc to exit play) but nothing in the UI hints at them. No shortcut overlay, no tooltip annotations on buttons.

**Fix:** Append shortcut text to button labels or tooltips. Optional: F1 help overlay listing all bindings.

### 16. CollapsingHeader state not persisted [low]

(From `editor-ui-state.md`) ImGui only saves window-level state in `imgui.ini`. Section expanded/collapsed state within panels (bake editor presets, render config sections) resets every session.

**Options:** `ImGui::GetStateStorage()` with manual save/load to rtcache, or store bools in config files.

### ~~17. Too many windows auto-show on startup~~ — resolved

Bake editor, converter, action progress moved to VS Code. Remaining ImGui windows: console, perf counters, render config.

### ~~18. Bake preset feedback missing~~ — resolved

VS Code bake editor panel shows active preset name.

### 19. Perf counter update stutter [low]

`ui.cpp:386` — `lock = 24` means values update every 24 frames. Creates visible jumps in displayed numbers.

**Fix:** Exponential moving average updated every frame. Smooths display without lag.

### 20. Selection outline requires render_scale [low, architectural]

`kryga_render_passes_draw.cpp:319-354` — Object outline only works when `render_scale` is enabled (composite pass dependency). Selection feedback silently disappears at native resolution.

### 21. Hardcoded layout values in gizmo editor [low]

- `gizmo_editor.cpp` — hardcoded drag speeds (0.1/1.0/0.01) and snap ranges
- (object_editor, script_editor, converter_window ImGui code deleted)

---

## Visual

### 22. No icons [low]

All toolbar items, tree nodes, and menu entries are text-only. No icon font or image-based icons. Reduces scannability.

### 23. Red theme contrast [low]

Red buttons at `(1.0, 0.0, 0.0, 0.4)` against `(0.06, 0.06, 0.06)` background have low contrast. Hover/active states are alpha-only changes (0.4 → 0.6 → 0.8), subtle. No visual hierarchy between primary and secondary actions.

### 24. Single font, two sizes [low]

Roboto-Medium at 28pt (normal) and 33pt (headers). No bold/italic variants. No monospace font for console or numeric fields.

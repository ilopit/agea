# CollapsingHeader state not persisted between sessions

ImGui only saves window-level state (position, size, collapsed) in `imgui.ini`. Tree node / `CollapsingHeader` state within windows is session-only.

**Affects:** Bake editor presets section, render config sections, any `CollapsingHeader` in editor panels.

**Options:**
- Store expanded/collapsed bools in the relevant config file (mixes UI state with config)
- Use `ImGui::GetStateStorage()` with manual save/load to rtcache
- Accept as session-only (current behavior)

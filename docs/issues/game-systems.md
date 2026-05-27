# Game Systems

## 1. Finalize game_system / game_session design [medium]

`game_system_manager` is wired into the engine loop and editor play-mode toggle, but `game_session` is defined and never instantiated. The boundary between "logic that belongs on components" vs "logic that belongs in a game_system" is undefined.

**Decide:**

- Which gameplay concerns (spawning, triggers, win conditions) live on **components** attached to level game objects.
- Which concerns (session state, progression, level transitions) remain in **game_system** subclasses that outlive a single level.
- Whether `game_session` survives as-is, gets slimmed down to only cross-level state, or gets replaced by component-based equivalents.

**Current state:** `game_system_manager` is created at engine init (`kryga_engine.cpp`), ticked every frame, and notified on play-mode toggle. `game_session` exists in `libs/game/` but nothing calls `manager.add<game_session>()`.

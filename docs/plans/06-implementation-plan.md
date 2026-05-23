# 06 — Implementation Plan

Milestone rollup across game, engine, and port. Each milestone has an exit gate. Do not advance to the next milestone until the gate passes.

Ballpark timelines assume solo, part-time pace. Numbers in weeks. Treat as relative ordering, not commitments.

## Milestones overview

| # | Name | Weeks | Depends on | Status (2026-05) |
|---|------|-------|-----------|------------------|
| M0 | Engine foundation for this game | ~4 | nothing | ~70% — core plumbing works; audio, save, state machine missing |
| M1 | Playable desktop prototype | ~4 | M0 | 0% — no puzzle code |
| M2 | Android build works | ~4 | M1 + Doc 05 | ~60% — build pipeline works; gestures, lifecycle pending |
| M3 | Vertical slice on Android | ~4 | M2 | 0% |
| M4 | Content & polish | ~6 | M3 | 0% |
| M5 | Shipping infrastructure | ~4 | M4 | 0% |
| M6 | Soft launch | rolling | M5 | 0% |

Total before soft launch: ~6 months part-time. Halve if full-time. Expect slippage of 30–50% — budget for it.

## M0 — Engine foundation for this game

**Goal:** engine has the subsystems this specific game needs, running on desktop.

Scope:

- Audio subsystem (miniaudio integration, SFX + ambient, focus handling).
- Player-facing UI system (custom minimal widgets).
- Save system (local JSON, profile + settings + per-puzzle state).
- Touch-capable input abstraction (desktop mouse + keyboard still works).
- Application state machine (menu / puzzle / settings).
- Event bus promotion.

**Exit gate:**

- A debug "play tone on click" scene runs with audio through the UI system, reading + writing save, via the app state machine, on Windows desktop.

Not in scope: Android, puzzle gameplay, content.

## M1 — Playable desktop prototype

**Goal:** first puzzle end-to-end on desktop. Tooling for puzzle authoring exists.

Scope:

- Puzzle data model and pack file format.
- Puzzle solver library (correctness-critical, unit-tested).
- Puzzle editor (ImGui window in engine.exe).
- 5 hand-authored 3×3×3 puzzles, using editor.
- Core puzzle scene: camera orbit, voxel selection, clue display, solved state, undo.
- Core UI: main menu → pack select (one pack) → puzzle → solved screen → back.

**Exit gate:**

- New user installs desktop build, clicks through menu, solves the first hand-authored puzzle, sees solved state, returns to menu. No crash, no confusion from a first-time viewer.

Notable: **this is the first time we know the core loop is fun or flat.** Be willing to rework Doc 03 based on this.

## M2 — Android build works

**Goal:** engine + game compiles and runs on Android hardware.

Scope:

- Execute Doc 05 Phases 0–3 (feasibility, build system, platform layer, Vulkan adaptation).
- Touch input gestures (tap, long-press, two-finger orbit, pinch) — upgrade from port plan's "multi-touch ignored initially".
- VFS APK backend.
- Lifecycle save/restore during onPause/onResume.
- AAB build pipeline on dev machine (not yet CI).

**Exit gate:**

- `./tools/build.sh android` (or equivalent) produces a signed AAB.
- AAB sideloads onto a real phone, launches, shows the main menu, reaches the puzzle scene, accepts taps, rotates with two fingers.
- Backgrounding and returning preserves puzzle state and does not crash.

## M3 — Vertical slice on Android

**Goal:** full loop polished on Android — one complete pack, plus settings.

Scope:

- First pack expanded: 20 puzzles across difficulty band.
- Settings screen: audio volumes, colorblind palette, large-targets toggle, reset progress.
- Pause / resume within a puzzle.
- Full audio set: taps, marks, carves, undo, solved, pack-complete, ambient loop.
- Safe area / notch layout respect.
- Orientation: portrait default, landscape does not crash.
- Tutorial puzzle (starter 1) — integrated, skippable.

**Exit gate:**

- External playtester (not the developer) installs the APK on their own phone, completes the Starter pack, writes a 3-sentence reaction with no prompting.
- Crash-free session rate 100% across playtester + developer sessions over one week.

## M4 — Content & polish

**Goal:** enough content to justify release; presentation feels finished.

Scope:

- Total of 3–4 packs × 20 puzzles (Starter + Easy + Medium + optional Hard).
- Puzzle generator (optional, if solver permits reliable unique-solution output).
- Shape gallery meta layer.
- Daily puzzle rotation.
- Theme palettes and per-pack visual identity.
- Tonemap post-process if low-poly reads flat on device.
- Localization plumbing + first three languages: JP, DE, FR.
- Particle / VFX pass: solved flourish and pack-complete celebration.

**Exit gate:**

- 60+ shippable puzzles exist in the build.
- Game is playable top-to-bottom in EN, JP, DE, FR.
- Developer plays from cold launch through pack completion without wincing at a specific UX flaw.

## M5 — Shipping infrastructure

**Goal:** turn a working game into a shippable app.

Scope:

- Crash reporting integration (Sentry lean).
- AAB CI pipeline (GitHub Actions + keystore in secrets).
- Play Console setup: app listing, screenshots, trailer (30–60 sec), store description in EN/JP/DE/FR.
- Privacy policy (required by Play Store), data safety declaration.
- Internal testing track set up, 5–10 testers recruited.
- If premium-pack monetization picked: Google Play Billing integration.
- If freemium picked: rewarded-ad SDK integration (AdMob or equivalent).

**Exit gate:**

- Internal testing build is live on Play Console, testers can install via Play Store link, and the game is compliant with Play policies (including data safety).
- A crash on any tester device appears in Sentry within minutes.

## M6 — Soft launch

**Goal:** validate discovery, retention, and monetization in the wild.

Scope:

- Open testing track or limited-region production launch — recommend JP + DE as first soft-launch markets for this genre (see Doc 02).
- Monitor crash reports, Play Console stats, store reviews.
- Fix crashes and blockers. Do **not** add features.
- Revisit monetization model if conversion data undermines the choice made in M3–M5.

**Exit gate to "v1.0 global launch":**

- Crash-free session ≥ 99.5% over 7 days.
- No blocking reviews (1-star with reproducible bugs) unaddressed for > 48 hours.
- At least 100 real installs.

## Dependencies at a glance

```
M0 ── M1 ── M2 ── M3 ── M4 ── M5 ── M6
        │           │           │
        │           │           └── monetization integration (billing OR ads)
        │           └── external playtest
        └── puzzle solver correctness (blocking)
```

## What is intentionally not here

- Post-launch content cadence (new packs, seasonal, etc.) — belongs in a post-v1 doc.
- iOS port (Doc 05, deferred).
- Second game on the engine (Doc 01 stretch goal).
- Cloud save, analytics SDK, IAP-beyond-billing, community features.

## Biggest scheduling risks

See Doc 07 for full list. High-impact on schedule:

- Custom UI system exceeding 2 weeks → cascades into M0 exit and every later milestone.
- Puzzle solver correctness bugs discovered late → M1 slips and content production blocks.
- Android lifecycle bug (surface loss + resume) eating a week in M2.
- Content production slower than hoped → either ship fewer packs in M4 or slip M5 hard.

Pre-decide: if any milestone exceeds its budget by 50%, stop and reassess scope rather than push on.

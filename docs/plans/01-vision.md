# 01 — Vision & Pivot

## One-liner

A simplified voxel sudoku game on a custom Vulkan engine — calm, 3D, low-poly, single-player.

## What this project is

Two intertwined products built by a solo developer:

1. **kryga** — a custom C++23 / Vulkan 1.2 engine with baked GI, clustered shading, and a package-based runtime.
2. **First game** — a simplified voxel sudoku puzzle title that ships on Android and validates the engine end-to-end.

Both ship together. The engine is the primary long-term asset; the game is the proof that the engine can ship.

## Pivot decision (as of 2026-04-20)

Earlier product writing framed this as a 3D low-poly match-3 targeting Women 30–55 in emerging markets. **That framing is dropped.** The game is a simplified voxel sudoku.

Consequences:

- Match-3 comps (Royal Match, Toon Blast, Gardenscapes) no longer apply.
- Audience, regions, monetization shape, session patterns all change (see Doc 02).
- `packages/tbs` (hex-grid TBS code) is **not** the game. Out of scope for the first title. Flagged for removal in Doc 04.
- Emerging-markets-first regional strategy is probably wrong for this genre. Re-evaluated in Doc 02.

## Why this genre

**Benefits:**

- Reuses engine strengths: 3D rendering, baked lighting, clean low-poly aesthetic all line up with the look Picross-style players expect.
- Logic-driven gameplay, not progression-driven → no live-ops, no whales, no FOMO mechanics to build.
- Much smaller competitive field than match-3 — niche but loyal audience.
- Content is small and structured (grids with clues) → amenable to a generator + solver pipeline once basics work.
- Session shape (3–15 min per puzzle) matches solo-dev content velocity better than an endless-runner or level-based match-3 that demands constant new content.

**Drawbacks to accept:**

- TAM is orders of magnitude smaller than match-3. "Scale" ambition has to be recalibrated.
- Niche genres reward quality and trust — hard to stand out without a polished first release.
- Puzzle design is a distinct skill. Solo dev means you are also the puzzle designer or you build a generator that must be correct.
- Picross-style buyers often expect no ads / premium feel. Freemium patterns from match-3 don't map cleanly.

## Why a custom engine

**Benefits:**

- Primary goal of the project is engine + shipping learning, not time-to-revenue.
- Engine reuse target: a small series of casual puzzle games can share kryga.
- Differentiated visuals: baked GI + low-poly is an aesthetic most Unity asset-flip puzzle games don't achieve.
- Full control over performance on target devices.

**Drawbacks to accept:**

- Months of platform glue (audio, UI, save, touch, AAB pipeline) that Unity/Godot would hand you.
- No asset store. Every art/audio/tooling gap is a build-it decision.
- Very real risk of never shipping. Mitigated by small scope and hard phase gates (Doc 06).

## Product pillars

1. **Calm.** No timers, no lives, no ads interrupting mid-puzzle. Tone over pressure.
2. **Satisfying feedback.** Chimes, clicks, small celebrations. SFX is a first-class feature, not decoration.
3. **Clean 3D look.** Low-poly meshes, baked GI, soft shadows, pastel palette. Readable at mobile screen sizes.
4. **Logic > luck.** Every puzzle has a unique solvable-by-deduction answer. No guessing required.
5. **Offline-first.** The game works on a plane, in a subway, with no network.

## Non-goals (v1)

- AAA fidelity or photorealism.
- Competitive / multiplayer / social features.
- Live-ops, seasonal events, battle passes.
- Heavy meta / collection / decor layers (Gardenscapes-style).
- Cross-platform launch. Android ships first; iOS deferred.
- Generative content at launch — manual puzzle set is acceptable for the first pack.

## What success looks like

Minimum viable success (shipping, not revenue):

- Game is installable from Google Play on a real mid-tier Android device.
- First puzzle pack (20 puzzles) is playable front-to-back: launch → menu → puzzle → solve → next.
- No crashes across suspend/resume, rotation, low-memory, background audio from other apps.
- Crash-free session rate ≥ 99% on supported device floor.
- At least 100 real installs and 20 completed first-puzzles from people the developer doesn't know personally.

Stretch:

- D1 retention measurable (not a target number yet — needs to be set after soft launch data).
- Second pack ships within 2 months of v1.
- Engine used for a second, different game prototype within 12 months.

## Relation to existing engine work

Already in place and directly useful: Vulkan render pipeline, BDA, clustered lights, baked lighting, shadow mapping, glTF import, ImGui editor, scene/level management, VFS + .apkg packaging.

Already in place but not used by this game: skeletal animation, hex grid / TBS code, DPSM shadow variant.

Not in place and required: audio, player-facing UI, save, touch input, gesture handling, AAB pipeline, crash reporting. See Doc 04.

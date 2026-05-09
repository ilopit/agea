# 03 — Game Design

First-pass design for the simplified voxel sudoku title. Expected to revise after playable prototype (M1).

## Core fantasy

You are carving a 3D shape out of a block of voxels using numeric clues. When you finish, the shape is something recognizable — a ship, a bird, a teapot — and the board is cleared with a soft chime.

Calm, tactile, no timer, no failure state.

## Core mechanic

Inspired by Picross 3D / voxel sudoku, simplified.

1. Puzzle starts as a solid cubic block of voxels: 3×3×3, 4×4×4, or 5×5×5.
2. Each row along each axis shows a numeric clue: the count of voxels that should remain in that row.
3. Player marks voxels as either **keep** (solid) or **carve away** (removed).
4. Puzzle is solved when the kept voxels match the target shape *and* all clue counts are satisfied.
5. Every puzzle has exactly one logically deducible solution. No guessing.

## Simplifications vs. classic voxel sudoku

- No "split" clues (the dot/circle notation from Picross 3D Round 2) in v1.
- Max grid size 5×5×5 for v1.
- Generous undo. Limited free hints per day (or rewarded-ad hints).
- Mistake marking is optional — can be toggled on/off. Default: on for easy, off for hard.
- No multi-color layers in v1.

These simplifications exist to reduce design and tutorial load. Later difficulty packs can reintroduce advanced rules.

## Controls (touch)

| Input | Action |
|-------|--------|
| Tap voxel | Cycle mark: unknown → keep → carve → unknown |
| Long-press voxel | Direct-set to "carve" |
| Two-finger drag | Rotate camera around puzzle |
| Pinch | Zoom (subtle — camera distance bounded) |
| Single-finger drag on edge | Slice layer view (hide front layers to see behind) |
| UI buttons | Undo, hint, pause, restart |

Desktop parity (for dev / tooling):
- Left click = tap, right click = long-press, mouse wheel = zoom, middle drag = rotate.

## Progression

### Pack-based, linear within a pack

- **Pack** = 20 puzzles of matched difficulty band, grouped by a visual theme (sea creatures, tools, birds, etc.).
- Puzzles unlock in order within a pack.
- Packs unlock linearly, but once unlocked a pack stays unlocked.
- Difficulty bands:
  - **Starter** (3×3×3, 8–12 puzzles, free).
  - **Easy** (3×3×3 and 4×4×4, 20 puzzles).
  - **Medium** (4×4×4, 20 puzzles).
  - **Hard** (5×5×5, 20 puzzles).
  - **Expert** (5×5×5 with split clues, post-v1).

### Completion state per puzzle

- Unsolved / solved / solved-without-hints / solved-perfect (no undos).
- Small icon next to puzzle thumbnail reflects best state achieved.

## Meta layer — intentionally thin

Heavy meta (decor, story, collection minigames) is out of scope for v1. Too much design and content cost. Instead:

- **Shape gallery.** Every solved puzzle reveals its finished 3D shape in a gallery. Players can rotate / inspect. Cheap to build, satisfying completion hook.
- **Pack completion rewards.** Purely cosmetic — a new theme color palette unlocks when a pack is 100%.
- **Daily puzzle.** One curated puzzle per day, cycling difficulty. Drives habit without forcing live-ops infrastructure.

No streaks, no leaderboards, no friends, no lives. These are hostile to the target audience.

## Monetization — direction, not decision

Two candidate models. Pick one before soft launch (M6).

### A. Premium pack model (Conceptis / Picross S style)

- Free: Starter pack (8–12 puzzles), one Easy pack partially unlocked (first 5 of 20), daily puzzle.
- IAP: per-pack $1.99–2.99, or season bundle (all current packs) $9.99–12.99.
- No ads, no rewarded video.
- Honest to the audience. Lower CAC tolerance — needs organic discovery.

### B. Freemium + rewarded hints

- Free: Starter + all Easy puzzles.
- Medium/Hard gated behind pack IAPs ($0.99–1.99).
- Rewarded ads: unlock a hint. Optional, never forced. No interstitials ever.
- Broader install funnel, lower ARPPU, higher complaint surface area.

**Lean:** A for v1. B if soft-launch data shows pack-IAP conversion is too thin to sustain content production.

## Content production

This is the real bottleneck.

- **v1 content target:** 3 packs × 20 puzzles = 60 puzzles, plus a Starter pack of ~10. Total ~70 puzzles.
- **Per-puzzle cost:** design + validate + playtest + select-representative-shape. Realistic estimate: 1–3 hours handcrafted once tooling exists, 10+ hours without tooling.
- **Tooling required before content production scales:**
  - Puzzle editor (ImGui-based, in-engine).
  - Puzzle solver — given a target shape, produce clue counts and verify unique solvability.
  - Puzzle generator — given dimensions and difficulty band, produce candidate puzzles for human review.
- **Sequencing:** M1 ships 5 hand-authored puzzles to prove the loop. Editor + solver built in M1/M2. Generator in M3/M4.

If the generator can't produce unique-solution puzzles reliably, fall back to pure handcraft and shrink launch content to 30 puzzles.

## Audio

Central to the pillar, unbuilt today.

- **Feedback sounds:** voxel tap, voxel mark, voxel carve, undo, invalid, solved, pack-complete.
- **Ambient:** 30–60 second looped track per theme pack, low-mix under gameplay.
- **Music:** none during gameplay (bed only). Light musical flourish on pack completion.
- **3D positional:** not needed for this game. Stereo pan + volume is enough.
- **Compressed format:** Ogg Vorbis for loops, short WAV for SFX (cheaper than decoding Ogg for one-shots on low memory).

Audio library decision (open): **miniaudio** for simplicity, **OpenAL Soft** if spatial is ever needed. Lean miniaudio.

## Visuals

- Low-poly voxel models. 3D baked GI on static meshes. Per-theme palette.
- Soft directional shadow + ambient from lightmaps.
- Camera: one stationary 3/4 perspective with user-rotatable orbit. No dynamic camera moves.
- UI: custom minimal widget system. Large tap targets. Typography-first, no skeuomorphic frames.
- Post-processing: none in v1. Tonemap may be added if HDR lightmap output looks off on device.

## Accessibility

- Colorblind palettes at launch (protan / deutan / tritan variants for voxel keep/carve marks — never rely on color alone).
- Large tap targets (min 48 dp).
- Optional mistake-highlight toggle.
- Optional "no-SFX" mode (pure ambient).
- No reaction-time gameplay anywhere.

## Open design questions

- Rotation control: free orbit vs. snap-to-face? (Snap probably feels better on small grids.)
- Should the "slice layer" view auto-activate when a clue is ambiguous from current angle, or always be manual?
- Hint design: reveal one correct voxel, or highlight the row with the next deducible clue? The latter teaches; the former rescues.
- Tutorial: integrated first puzzle or separated "how to play"? Probably integrated, skippable.
- Daily puzzle: how to source? Handcrafted pool vs. generator output. Ties into content-production decision.

Open questions are tracked long-term in Doc 07.

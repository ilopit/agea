# Design Docs — Index

Living design docs for the kryga engine + first shipped game.

Created: 2026-04-20

## Structure

| # | Doc | Purpose |
|---|-----|---------|
| 01 | [Vision](01-vision.md) | Product + engine vision. What success looks like. |
| 02 | [Target player & market](02-target-player.md) | Audience profile, regions, comps. |
| 03 | [Game design](03-game-design.md) | Core mechanics, progression, meta, monetization shape. |
| 04 | [Engine roadmap](04-engine-roadmap.md) | What to build, defer, cut — keyed to the engine audit. |
| 05 | [Mobile port](05-mobile-port.md) | Full mobile port plan — policy, execution phases, file changes, exit criteria. |
| 06 | [Implementation plan](06-implementation-plan.md) | Milestone rollup with phase gates and exit criteria. |
| 07 | [Risks & open questions](07-risks-open-questions.md) | Known risks, unresolved decisions, deferred choices. |

## Reading order

For a first-time reader: 01 → 02 → 03 → 04 → 05 → 06 → 07.
For status check: 06 (milestones) → 07 (open questions).
For scope discussion: 01 + 04 + 06.

## Relationship to other repo docs

- `CLAUDE.md` — engine architecture reference for contributors. Kept separate; updates there are not automatic when these docs change.

## Decisions already locked

- **Genre:** simplified voxel sudoku.
- **`packages/tbs`** (hex grid): out of scope for the first game. Exclusion tracked in Doc 04.
- **Render API:** Vulkan 1.2 + BDA only. No GL/ES fallback.
- **First platform:** Android. iOS deferred (Doc 07).
- **Engine reuse goal:** kryga powers this game and future casual puzzle titles.

## Open decisions blocking the docs

See Doc 07 for the full list. High-impact:

- Puzzle content strategy: handcrafted, generated+validated, or hybrid.
- Monetization: premium packs vs. freemium + rewarded ads.
- Audio library: miniaudio vs. OpenAL Soft.
- UI system: custom minimal vs. integrate RmlUi / similar.

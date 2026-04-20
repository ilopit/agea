# 07 — Risks & Open Questions

Living list. Update as decisions are made or risks materialize.

## Risks

### Technical

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| BDA + Vulkan 1.2 min-spec excludes too many target devices | Medium | Medium | Accept narrower reach (genre-aligned anyway). If soft launch shows real device rejections, build SSBO fallback per Doc 05 Phase 3. |
| Android lifecycle edge cases (surface loss, audio focus, low-memory kill) eat weeks in M2/M3 | High | High | Real-device testing from day one of M2. Do not rely on emulator. Budget buffer in M2. |
| Custom UI system underdelivers → UX regressions | Medium | Medium | 2-week timebox in M0. If exceeded, fall back to RmlUi. Don't sink a month before deciding. |
| Puzzle solver has correctness bugs → puzzles with >1 solution ship | Medium | High | Solver is the single most test-worthy component. Table-driven unit tests against hand-solved puzzles, starting in M1. Every puzzle in a pack runs the solver in CI before release. |
| `packages/tbs` drift or build breakage as it rots | Low | Low | Exclude from main build target at M0. Revisit or delete at M5. |
| Miniaudio gaps on Android (focus, backgrounding, glitches on specific SoCs) | Medium | Medium | Verify focus + backgrounding in M0 on one real device before building the rest of the audio surface. |
| Content pipeline not scaling — 60 puzzles take longer than budgeted | High | Medium | Generator as early as M1/M2. If generator doesn't produce unique solutions reliably, shrink launch to 30 puzzles rather than slip. |
| Custom engine absorbs time that should go to game | High | High | Hard rule: no non-P0 engine work during M3–M6 without a concrete game-facing reason. |

### Product / market

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Voxel sudoku TAM smaller than hoped → install ceiling is low | High | Medium | Acknowledge up-front: this project's success is shipping + learning, not revenue. See Doc 01 success definition. |
| Organic discovery fails → 0 installs despite good product | Medium | High | Invest in store-page presentation (screenshots, trailer) in M5. Plan at least one Reddit / HN / puzzle-community post around launch. |
| Monetization model mismatch (freemium vs. premium) | Medium | Medium | Delay binding decision until soft launch data (Doc 03). Soft-launch cheap → iterate. |
| Tutorial failure — new players don't grasp the mechanic | Medium | High | External playtest at M3 exit gate. If testers fail, redesign tutorial before M4. |

### Solo-dev

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Attrition / loss of motivation before shipping | Medium | Very High | Small scope, short milestones, each exit gate is a shippable-feeling artifact. Ship Starter pack as a public demo at M3 even if rest of game isn't ready. |
| "One more feature before shipping" spiral | High | High | Milestone exits are hard gates. Cutting features > slipping dates. |
| Re-architecting the engine mid-game-development | Medium | High | Engine work post-M1 is defensive (bug fixes) only, unless blocking a game feature. |
| No code review → architecture decisions calcify | High | Medium | Write decisions down (these docs). Revisit them at milestone boundaries. Accept you'll regret some choices and won't fix them. |
| Burnout from audio/UI work that isn't fun | Medium | High | Batch the unfun work. One week of UI → one week of puzzle mechanics. Don't chain three unfun weeks. |

## Open questions

Each has a latest-sensible-decision deadline. Decide by then or commit to defaulting.

### Product

1. **Monetization: premium-pack vs. freemium + rewarded hints.**
   - Default if undecided by M5: premium-pack (Doc 03 recommended, lower infrastructure cost).
   - Decide by: start of M5.
2. **Tutorial format: integrated first puzzle vs. separated explainer.**
   - Default: integrated.
   - Decide by: mid-M3.
3. **Daily puzzle source: handcrafted pool vs. generator.**
   - Depends on generator decision (below).
   - Decide by: M4 start.
4. **Meta layer scope: shape gallery only, or also unlockable palettes/themes.**
   - Default: shape gallery + palette unlocks (Doc 03).
   - Decide by: M4 start.
5. **Target regions for soft launch.**
   - Recommended: JP + DE (Doc 06).
   - Decide by: M5.

### Technical

6. **Audio library: miniaudio vs. OpenAL Soft vs. FMOD/Wwise.**
   - Default: miniaudio.
   - Decide by: M0 start.
7. **UI library: custom minimal vs. RmlUi.**
   - Default: custom minimal, 2-week timebox, fall back to RmlUi.
   - Decide by: M0 week 2 checkpoint.
8. **Crash reporting backend: Sentry vs. Firebase Crashlytics.**
   - Default: Sentry (privacy, no Firebase pull-in).
   - Decide by: M5 start.
9. **Android windowing: SDL2 vs. GameActivity + native window.**
   - Default: SDL2 (already used).
   - Decide by: M2 start.
10. **Asset delivery: APK assets vs. Play Asset Delivery.**
    - Default: APK assets while total size < 100 MB.
    - Decide by: M5.
11. **Puzzle generator: build in v1 or handcraft only.**
    - Depends on solver maturity at M2.
    - Decide by: M3 start.

### Scope / business

12. **iOS port scheduling.**
    - Default: deferred indefinitely, revisit post-Android-launch.
    - Decide by: 1 month post-Android-launch.
13. **`packages/tbs` fate: keep in repo, delete, or fork to a separate repo.**
    - Default: keep in repo, exclude from main build; revisit at M5.
    - Decide by: M5.
14. **Engine-reuse-for-series claim: realistic timeline?**
    - Open. Depends on how much of v1's engine survives to v2.
    - Decide by: 6 months post-Android-launch.
15. **Editor distribution: internal only, or ship as a puzzle-creator side feature?**
    - Default: internal only for v1.
    - Decide by: M4.
16. **Localization breadth beyond EN/JP/DE/FR.**
    - Default: Korean + Spanish as post-launch wave.
    - Decide by: M5.

## Explicitly deferred

Not decisions yet, just noted so they don't accidentally creep into v1:

- Cloud save.
- Analytics SDK beyond Play Console basics.
- Ads SDK if premium-pack model ships.
- IAP SDK if free-only model ships.
- Leaderboards / social / multiplayer (permanent non-goals for this game).
- Skeletal animation productization (code stays, not used here).
- Post-processing suite beyond possible tonemap.
- Hot-reload.

## Revision policy

- Add a dated entry when a risk materializes (or is retired) and when an open question is answered.
- Do not rewrite history — decisions are valuable even when they age badly.
- At each milestone exit, spend 30 minutes reviewing this doc. Update defaults and deadlines.

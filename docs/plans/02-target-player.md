# 02 — Target Player & Market

Rewritten for the voxel sudoku genre. Previous match-3 audience analysis is discarded.

## Who the player is

### Primary — "Picross / Sudoku regulars"

- Age 35–65, mixed gender. Sudoku skews slightly older and female; Picross/nonogram skews slightly male — voxel sudoku sits between.
- Plays daily or near-daily. Treats puzzles like a crossword — a mental stretch, not entertainment.
- Pays for quality. Has historically bought Picross 3D, Conceptis packs, Good Sudoku+, Picross S on Switch.
- Low tolerance for ads, FOMO prompts, "energy" systems. High tolerance for learning curves.
- Evening / commute / coffee-break sessions. 3–15 minutes, 1–3 puzzles per sit.
- Values mastery and completion. Wants to finish the pack, not chase streaks.

### Secondary — "Casual logic puzzle" players, 25–45

- Play a mix: Sudoku, Solitaire, Wordle, I Love Hue, Threes.
- More price-sensitive than the primary segment. More open to rewarded ads for hints.
- Install volume is higher than the primary; retention is lower.
- Discovery via "puzzle" category browsing and editorial features.

### Tertiary — Family / kids and older-adult (65+)

- Voxel aesthetic, no timers, and no text-heavy UI make this genre naturally accessible.
- Small revenue contribution individually; meaningful in aggregate.
- Requires large tap targets, high-contrast mode, colorblind-safe palette, and optional sound (not all environments allow audio).

## What the player is *not*

- Not the match-3 audience. Royal Match retention tactics (streaks, lives, FOMO events) will repel this audience.
- Not a competitive or social gamer. No leaderboards needed.
- Not a graphics enthusiast. They will not tolerate a 180 MB download for a puzzle game.

## Regional strategy — revised

Match-3 emerging-markets-first plan does not fit this genre.

### Likely primary regions

- **Japan** — Picross is a cultural fixture. Premium pricing tolerated. App Store ranking matters; Google Play a distant second.
- **United States / United Kingdom / Germany** — Large Picross / Sudoku apps already monetize well here. English-default is fine for v1.
- **South Korea** — Strong puzzle genre penetration. Android-dominant on Samsung devices.

### Likely secondary regions

- **France, Italy, Spain** — localize if volume warrants.
- **Nordics** — small but high-ARPU puzzle audience.

### Deprioritized vs. previous plan

- India, Brazil, Indonesia, Philippines, Mexico.
- Volume exists, but genre penetration is low and willingness to pay for a niche puzzle is low.
- Not excluded — the BDA min-spec is already narrowing the device floor in these markets, so coincidental reach is fine; targeted launch marketing is not.

### Language priorities

v1 ships in English only. First localization wave: **Japanese, German, French.** Spanish and Korean follow. Hindi/Portuguese/Bahasa Indonesia are not priority for this genre.

## Device assumptions

Driven by the engine min-spec (Vulkan 1.2 + BDA):

- Android 11+.
- Adreno 640+ or Mali-G77+.
- 3+ GB RAM.
- Roughly: Snapdragon 855+ / Exynos 990+ class devices.

This is mid-to-upper-tier Android. It aligns naturally with the developed-markets demographic we're targeting. It would have been a problem for the match-3 plan; for voxel sudoku it is close to a non-issue.

## Comparable titles

| Title | Platform | Note |
|-------|----------|------|
| Picross 3D / Picross 3D Round 2 | Nintendo DS/3DS | Closest mechanical reference. Handcrafted puzzles, premium. |
| Voxel Sudoku (mobile apps) | iOS/Android | Direct genre comps. Mostly rudimentary UI and presentation. |
| Conceptis Picross series | iOS/Android | Freemium with pack IAPs. Reliable monetization template. |
| Good Sudoku | iOS | Premium tier, strong UX, smart hinting — UX reference. |
| I Love Hue / Hue Two | iOS/Android | Calm puzzle with strong aesthetic — tone reference, not mechanics. |
| Monument Valley 1/2 | iOS/Android | Premium 3D puzzle. Art-led. Aspirational presentation reference. |

## Player persona (one)

**"Yuki, 48, Tokyo."** Commutes 45 minutes each way. Plays one Picross puzzle per commute leg. Owns a Pixel 7a. Has spent ~¥3,000 on puzzle apps in the last year. Hates ads mid-puzzle. Would buy a quality voxel-sudoku game for ¥600–¥1,200. Recommends apps to two coworkers who also commute.

This persona is the one the game should feel designed for. The casual 25–45 segment is gravy.

## Key implications for design and business

- Premium or low-friction freemium — not whale-driven.
- Quality bar on presentation is high relative to the audience's small TAM. This is genre-typical.
- Localization matters early. JP/DE/FR before EN-only mass marketing.
- "Emerging markets first" marketing plan is inverted: **developed markets first, emerging markets as incidental reach.**
- Ads, if present, must be rewarded-for-hint only. Interstitials will kill reviews.
- Store listing screenshots and a short trailer matter more than ASO keyword spam in this genre.

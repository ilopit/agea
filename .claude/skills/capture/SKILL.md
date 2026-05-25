---
name: capture
description: Retrieve the last H-key captured screenshot from the running engine. Checks engine is alive first, then returns the image.
allowed-tools: mcp__kryga-scene__kryga_ping, mcp__kryga-scene__kryga_screenshot
---

Retrieve the last user-captured screenshot (H-key) from the running engine.

## Instructions

1. Ping the engine via `kryga_ping` to confirm it is running and responsive
2. If ping fails, tell the user the engine is not running and stop
3. Call `kryga_screenshot` with `get_last: true`
4. Return the captured image to the user — do NOT analyze or comment on it unless the user asks

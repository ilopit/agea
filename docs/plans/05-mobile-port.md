# 05 — Mobile Port

Policy layer. Execution lives in [`plans/android_port.md`](../../plans/android_port.md) — do not duplicate it here.

## Scope

### In scope for v1

- **Android.** First and only shipped platform for v1.
- Minimum spec driven by engine requirements:
  - Android 11+ (API 30+).
  - Vulkan 1.2.
  - Buffer Device Address support (Adreno 640+ / Mali-G77+).
  - 3+ GB RAM.
  - arm64-v8a only. No armeabi-v7a, no x86.

### Out of scope for v1 (deferred)

- **iOS.** Deferred. Adds MoltenVK, Xcode + App Store Connect workflow, separate signing, separate testing hardware. Revisit after Android ships and the loop is validated. Tracked in Doc 07.
- **ChromeOS / Android tablets.** Landscape layout not a v1 priority. If it happens, it happens — do not block on tablet polish.
- **Cloud streaming platforms** (GeForce Now, xCloud): not applicable; single-player offline puzzle.

## Relation to `plans/android_port.md`

That doc is the working implementation plan: phases, file changes, tooling. It is **authoritative** for execution.

This doc is **policy** — what we're choosing, what we're deferring, and why — and what this specific game requires from the port.

### What this game needs from the port that the port plan already covers

All of the following are in `plans/android_port.md` and need no duplication here:

- NDK toolchain (Phase 1.1).
- Gradle / JNI / AAB pipeline (Phase 1.2).
- SDL2 cross-compile or NativeActivity (Phase 2.1 / 2.2).
- VFS APK asset backend (Phase 2.3).
- Vulkan surface creation for Android (Phase 3.1).
- Swapchain + FIFO present + pre-rotation handling (Phase 3.3).
- Frame-rate cap + background pause + thermal response (Phase 4.2).
- ASTC compression (Phase 4.3).
- BDA feature-check and fallback decision (Phase 3.2).

### What this game specifically requires that deserves calling out

- **Touch input with multi-touch gestures** (not just tap). Two-finger orbit and pinch zoom are in the control scheme (Doc 03). `android_port.md` Phase 2.4 proposes "multi-touch initially ignored" — that is not acceptable for this game. Upgrade multi-touch to P0.
- **Portrait-first, rotation allowed.** Most Picross-audience players hold portrait. Landscape must not crash. Orientation change handled, not locked.
- **Audio focus handling.** OS compliance + audience expectation (players will have music / podcasts playing). Covered by audio subsystem (Doc 04) but must be wired to Android's `AudioManager.OnAudioFocusChange`.
- **Background / foreground lifecycle preserves puzzle state.** Puzzle progress must survive app being backgrounded for an hour (save on pause, restore on resume). Save system (Doc 04) must be called during `onPause`.
- **No GPU work while in background.** Strict — drains battery fast otherwise.
- **Small APK.** Target <60 MB initial download, <150 MB install. ASTC + shared mesh + limited audio asset budget.

## Platform integration status (from audit)

All currently at 1 (absent):

- Vulkan surface recreation on suspend/resume.
- Orientation change handling.
- Multi-touch + gestures.
- Safe area / notch handling.
- Back button / navigation gestures.
- Audio ducking.
- Battery / thermal awareness.

Collectively: the engine is a desktop demo. Getting it to a shippable Android state is the single largest block of work ahead. Scheduling in Doc 06.

## Key technical decisions to make before execution

Each has an open issue entry in Doc 07 too.

1. **SDL2 vs. GameActivity + native window.** `android_port.md` leaves this open. Lean SDL2 initially — less code for lifecycle handling and kryga already uses it. Switch to GameActivity only if SDL2 proves fragile or the audio/input path forces it.
2. **Asset delivery.** APK assets vs. Play Asset Delivery on first run. For a <60 MB game with no large media, APK assets are simpler. Play Asset Delivery becomes worth it only if total asset size crosses 100 MB.
3. **Crash reporting backend.** Sentry self-hostable, Firebase Crashlytics easy but ties to Firebase SDK. Lean Sentry for v1 — avoid Google SDK footprint unless Play Games Services / IAP forces it in.
4. **Analytics.** Defer (Doc 04). Play Console stats are acceptable for v1.
5. **Signing + CI.** Keystore stored in GitHub Actions secrets; CI produces signed AAB on tag push. No manual signing from dev machine in the critical path.

## Test devices

Minimum physical device coverage to call the port "shipped":

- One Pixel-class device (Snapdragon 7xx/8xx, Adreno) at min-spec.
- One Samsung Galaxy Sxx (Exynos + Mali OR Snapdragon) — distinct GPU vendor.
- One lower-spec device *at* the device floor to validate min-spec claims.

Emulators are not sufficient. Vulkan BDA behavior and thermal/lifecycle behavior only reproduce on real hardware.

## iOS — what it would cost later

Not committing, just noting for eyes-open planning:

- MoltenVK layer (mostly works, some extension gaps — BDA is supported on recent MoltenVK).
- Xcode project, CocoaPods or SwiftPM, Apple Developer account + annual fee.
- Swift / Objective-C bridge for app lifecycle and AppDelegate.
- Separate input + audio integration path.
- App Store Connect review process (stricter than Play).
- Estimated 6–10 weeks of work on top of a shipped Android version.

If Android v1 gets any commercial or audience signal, iOS is a strong follow-on. Decision deferred.

## Exit criteria for "mobile port done" (v1)

- AAB signed and uploaded to Play Console internal testing track.
- First puzzle pack playable end-to-end on two real devices covering distinct GPU vendors.
- No crash across: cold start, suspend/resume, rotation, low-memory kill + relaunch, incoming phone call, audio focus loss, airplane-mode toggle.
- Crash-free session rate ≥ 99% across an internal testing cohort of at least 10 users over 7 days.

# Kryga VS Code editor integration

Phase 1 (polling frame transport). The engine runs headless when launched
with `--editor-ipc <name>`, publishes every rendered frame into a shared-
memory region, and the VS Code webview polls that region at 60 Hz.

Phase 2 replaces the poll loop with a signaled wait and adds the input
return path.

## Engine CLI (Phase 0)

```
engine_app --editor-ipc <name>        # Headless, offscreen render, IPC placeholder
engine_app --dump-first-frame <path>  # Headless, render warm-up burst, save PNG, exit
engine_app --headless-width <W>       # Headless render target width  (default 1024)
engine_app --headless-height <H>      # Headless render target height (default 1024)
engine_app --wait-for-debugger        # Print PID and block until debugger attaches
```

## Shared-memory name scheme (reserved for Phase 1)

The `<name>` passed to `--editor-ipc` is a bare identifier (letters, digits,
`_`, `-`). Each OS derives concrete primitive names from it:

| OS       | Shared memory region         | Frame-ready event / semaphore | Input-available event / semaphore |
| -------- | ---------------------------- | ----------------------------- | --------------------------------- |
| Linux    | `/kryga_editor_<name>`       | `/kryga_editor_<name>_fr`     | `/kryga_editor_<name>_ia`         |
| macOS    | `/kryga_editor_<name>`       | `/kryga_editor_<name>_fr`     | `/kryga_editor_<name>_ia`         |
| Windows  | `Local\kryga_editor_<name>`  | `Local\kryga_editor_<name>_fr` | `Local\kryga_editor_<name>_ia`   |

The leading `/` on POSIX and the `Local\` prefix on Windows are added by the
platform layer, not by callers. POSIX names are limited to 30 characters total
(SHM_NAME_MAX on macOS), so `<name>` must fit within ~14 ASCII characters for
cross-platform safety.

Launcher convention (TS side launches engine):

```
name = "session_" + process.pid.toString(16);
engine_app --editor-ipc ${name}
```

The engine unlinks stale shared-memory regions matching its own `<name>` at
startup, before creating a new one. The launcher (or the user) is responsible
for picking a name that does not collide with other live sessions.

## VS Code extension

Located in `editor/vscode/`.

```
editor/vscode/
  src/               TypeScript extension source
  media/             Webview HTML + viewport.js (WebGL blitter)
  native/            N-API addon (cmake-js build)
    src/addon.cpp    JS bindings for the shm reader
    CMakeLists.txt   Reuses libs/editor_ipc/ for the shm wrapper
```

Build and run:

```
cd editor/vscode/native
npm install                     # Builds the native .node via cmake-js
cd ..
npm install && npm run compile  # Compiles the TS extension
```

Open the `editor/vscode/` folder in VS Code and press F5 to launch an
Extension Development Host. In the host, run `> Kryga: Open Viewport`
(Ctrl+Shift+P) and enter the same `<name>` you passed to the engine's
`--editor-ipc` flag. The viewport panel attaches to the shared-memory
region and displays published frames.

### Frame pipeline (Phase 1)

```
engine draw_headless
   └─ render graph writes main_pass.color[frame_idx]  (VkImage)
        └─ frame_publisher.publish()
             ├─ vkCmdCopyImageToBuffer  →  host-visible staging buffer
             ├─ memcpy (BGRA→RGBA if needed)  →  shm slot pixel blob
             └─ atomic store latest_ready_slot, fetch_add frame_counter

addon.readHeader / getSlotBuffer  (polled every 16ms from ext host)
   └─ Buffer.from(slice)   — copy out of mmap while publisher is locked out
        └─ postMessage     — structured-clone into webview

webview.onmessage('frame')
   └─ gl.texSubImage2D  →  draw fullscreen triangle
```

Frames are therefore copied three times between GPU and canvas (staging →
shm → post-message → WebGL texture). Phase 1 accepts this; Phase 2 adds the
signaled wait but does not reduce copies. The copy count is noted so it's
easy to find when profiling later.

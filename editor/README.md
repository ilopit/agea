# Kryga VS Code editor integration

Phase 0 scaffold. The engine runs headless when launched with `--editor-ipc
<name>`; later phases add the actual frame-publishing and input transports.

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

Located in `editor/vscode/`. Phase 0 contains only project scaffolding
(TypeScript build config and a stub `kryga.openViewport` command). The N-API
addon and webview frame consumer land in Phase 1.

Build and run:

```
cd editor/vscode
npm install
npm run compile
```

Open the folder in VS Code and press F5 to launch an Extension Development
Host.

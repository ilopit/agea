---
name: engine
description: Control the running engine instance - kill, ping, sync files, or check status. Use when user wants to restart the engine, hot-reload shaders, or check if engine is running.
argument-hint: "<kill|ping|sync|status> [args...]"
allowed-tools: Bash
---

Control the running engine via `tools/engine_control.py`.

## Commands

```bash
# Kill running engine instance
python tools/engine_control.py kill

# Check if engine is alive
python tools/engine_control.py ping

# Sync a file (hot-reload shaders, scripts)
python tools/engine_control.py sync <file_path>

# Show discovery info (PID, port)
python tools/engine_control.py status
```

## How it works

1. The engine starts a TCP+JSON-RPC server (editor builds only) and writes a
   discovery file at `<project_root>/tmp/editor_rpc.json` containing `{pid,
   port, version}`. The port is OS-picked, not fixed.
2. `engine_control.py` reads the discovery file and connects to the port.
3. The Python script frames JSON-RPC 2.0 messages with LSP-style
   `Content-Length: N\r\n\r\n<body>` and dispatches to the matching server
   handler:
   - `ping` → engine echoes the params
   - `sync.reload` → engine queues a sync action and blocks until the main
     thread processes the file (5s timeout)
   - `kill` is local — uses the PID from the discovery file + taskkill /
     SIGTERM, no RPC.

## Supported sync file types

- `.vert`, `.frag` — GLSL shaders (triggers recompilation)
- `.lua` — Lua scripts

## Examples

```bash
# Kill and restart
python tools/engine_control.py kill
tools/run.sh kryga_editor.exe

# Hot-reload a shader (path must be absolute)
python tools/engine_control.py sync F:/dev/kryga/4/resources/packages/base.apkg/class/shader_effects/se_simple_texture.frag

# Check status
python tools/engine_control.py status
```

## Instructions

1. Run the appropriate command based on user request
2. If `kill` fails with "Process not found", engine already exited — proceed
3. If `ping` fails, engine is not running — inform user
4. If `sync` fails, check if engine is running first with `ping`
5. After killing, wait ~1 second before starting new instance

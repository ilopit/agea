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

# Show session info (PID, port)
python tools/engine_control.py status
```

## Options

- `--config`, `-c` — Build config: Debug (default) or Release
- `--port`, `-p` — Override HTTP port (default: 10033)
- `--session`, `-s` — Path to session.json (auto-detected)

## How it works

1. Engine writes `rtcache://session.json` on startup with PID and port
2. Python scripts read this file to find/communicate with engine
3. Session file is at `build/project_<Config>/rtcache/session.json`

## Supported sync file types

- `.vert`, `.frag` — GLSL shaders (triggers recompilation)
- `.lua` — Lua scripts

## Examples

```bash
# Kill and restart
python tools/engine_control.py kill
tools/run.sh engine_app.exe

# Hot-reload a shader
python tools/engine_control.py sync resources/shaders/pbr.frag

# Check status
python tools/engine_control.py status
```

## Instructions

1. Run the appropriate command based on user request
2. If `kill` fails with "Process not found", engine already exited — proceed
3. If `ping` fails, engine is not running — inform user
4. If `sync` fails, check if engine is running first with `ping`
5. After killing, wait ~1 second before starting new instance

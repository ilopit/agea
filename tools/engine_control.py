#!/usr/bin/env python3
"""
Engine control utilities — kill running instances, send sync commands, ping.

Talks to the engine over its TCP+JSON-RPC server. Discovery file at
<project_root>/tmp/editor_rpc.json contains {pid, port}. The engine writes
this when it starts (editor builds only) and removes it on shutdown.
"""

import argparse
import json
import os
import socket
import sys
from pathlib import Path


def get_project_root() -> Path:
    """Project root (parent of tools/)."""
    return Path(__file__).parent.parent


def discovery_path() -> Path:
    return get_project_root() / "tmp" / "editor_rpc.json"


def load_discovery() -> dict | None:
    p = discovery_path()
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text())
    except (json.JSONDecodeError, OSError) as e:
        print(f"Error reading {p}: {e}", file=sys.stderr)
        return None


def _send(sock: socket.socket, msg: dict) -> None:
    body = json.dumps(msg).encode("utf-8")
    sock.sendall(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body)


def _recv_until_id(sock: socket.socket, want_id: int) -> dict:
    buf = b""
    while True:
        # Read until \r\n\r\n
        while b"\r\n\r\n" not in buf:
            chunk = sock.recv(4096)
            if not chunk:
                raise ConnectionError("engine closed connection")
            buf += chunk
        head, _, rest = buf.partition(b"\r\n\r\n")
        length = 0
        for line in head.split(b"\r\n"):
            if line.lower().startswith(b"content-length:"):
                length = int(line.split(b":", 1)[1].strip())
        while len(rest) < length:
            chunk = sock.recv(length - len(rest))
            if not chunk:
                raise ConnectionError("engine closed connection mid-frame")
            rest += chunk
        msg = json.loads(rest[:length].decode("utf-8"))
        buf = rest[length:]
        if msg.get("id") == want_id:
            return msg
        # Skip notifications (log lines, etc.) and other-id responses.


def rpc_call(method: str, params: dict | None = None, timeout: float = 8.0) -> dict:
    info = load_discovery()
    if info is None:
        raise FileNotFoundError(f"no discovery file at {discovery_path()}")
    s = socket.create_connection(("127.0.0.1", info["port"]), timeout=timeout)
    s.settimeout(timeout)
    try:
        _send(s, {"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}})
        return _recv_until_id(s, 1)
    finally:
        s.close()


def kill_engine() -> bool:
    info = load_discovery()
    if info is None:
        print("No discovery file. Engine may not be running.", file=sys.stderr)
        return False
    pid = info.get("pid")
    if pid is None:
        print("No PID in discovery file.", file=sys.stderr)
        return False
    try:
        os.kill(pid, 0)
    except OSError:
        print(f"Process {pid} not found. Engine may have already exited.")
        discovery_path().unlink(missing_ok=True)
        return False
    try:
        if sys.platform == "win32":
            import subprocess
            subprocess.run(["taskkill", "/F", "/PID", str(pid)], check=True)
        else:
            import signal
            os.kill(pid, signal.SIGTERM)
        print(f"Killed engine (PID {pid})")
        return True
    except Exception as e:
        print(f"Failed to kill process {pid}: {e}", file=sys.stderr)
        return False


def ping() -> bool:
    try:
        resp = rpc_call("ping", {"hello": "engine_control"})
    except Exception as e:
        print(f"Engine not responding: {e}", file=sys.stderr)
        return False
    print(f"Engine alive: {json.dumps(resp.get('result'))}")
    return True


def send_sync(path: str) -> str | None:
    try:
        resp = rpc_call("sync.reload", {"path": path}, timeout=10.0)
    except Exception as e:
        print(f"sync.reload failed: {e}", file=sys.stderr)
        return None
    if "error" in resp:
        print(f"Engine error: {resp['error']}", file=sys.stderr)
        return None
    result = resp.get("result", {}).get("result", "")
    if result:
        print(f"Sync response: {result}")
    else:
        print("Sync ok")
    return result


def main():
    parser = argparse.ArgumentParser(description="Engine control utilities")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("kill", help="Kill running engine instance")
    sub.add_parser("ping", help="Check if engine is alive")
    sync_p = sub.add_parser("sync", help="Reload a file (lua/vert/frag)")
    sync_p.add_argument("file", help="Absolute path to the file")
    sub.add_parser("status", help="Show discovery info")
    args = parser.parse_args()

    if args.command == "kill":
        sys.exit(0 if kill_engine() else 1)
    elif args.command == "ping":
        sys.exit(0 if ping() else 1)
    elif args.command == "sync":
        sys.exit(0 if send_sync(args.file) is not None else 1)
    elif args.command == "status":
        info = load_discovery()
        if info:
            print(json.dumps(info, indent=2))
        else:
            print("No discovery file (engine not running, or game-only build)")
            sys.exit(1)


if __name__ == "__main__":
    main()

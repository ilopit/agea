#!/usr/bin/env python3
"""
Engine control utilities - kill running instances, send sync commands.
Session info is read from rtcache://session.json written by the engine.
"""

import argparse
import json
import os
import sys
import urllib.request
import urllib.error
from pathlib import Path

DEFAULT_PORT = 10033
SESSION_FILENAME = "session.json"


def get_project_root() -> Path:
    """Get the project root directory (parent of tools/)."""
    return Path(__file__).parent.parent


def find_session_file(config: str = "Debug") -> Path | None:
    """
    Find the session file in the build output.
    Looks in build/project_<config>/rtcache/session.json
    """
    root = get_project_root()
    session_path = root / "build" / f"project_{config}" / "rtcache" / SESSION_FILENAME
    if session_path.exists():
        return session_path

    # Try other configs
    build_dir = root / "build"
    if build_dir.exists():
        for subdir in build_dir.iterdir():
            if subdir.is_dir() and subdir.name.startswith("project_"):
                candidate = subdir / "rtcache" / SESSION_FILENAME
                if candidate.exists():
                    return candidate
    return None


def load_session(session_path: Path | None = None, config: str = "Debug") -> dict | None:
    """Load session info from file."""
    if session_path is None:
        session_path = find_session_file(config)

    if session_path is None or not session_path.exists():
        return None

    try:
        with open(session_path, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError) as e:
        print(f"Error reading session file: {e}", file=sys.stderr)
        return None


def kill_engine(session_path: Path | None = None, config: str = "Debug") -> bool:
    """Kill the running engine instance using PID from session file."""
    import signal

    session = load_session(session_path, config)
    if session is None:
        print("No session file found. Engine may not be running.", file=sys.stderr)
        return False

    pid = session.get("pid")
    if pid is None:
        print("No PID in session file.", file=sys.stderr)
        return False

    try:
        # Check if process exists
        os.kill(pid, 0)
    except OSError:
        print(f"Process {pid} not found. Engine may have already exited.")
        # Clean up stale session file
        if session_path:
            session_path.unlink(missing_ok=True)
        return False

    try:
        if sys.platform == "win32":
            import subprocess
            subprocess.run(["taskkill", "/F", "/PID", str(pid)], check=True)
        else:
            os.kill(pid, signal.SIGTERM)
        print(f"Killed engine (PID {pid})")
        return True
    except Exception as e:
        print(f"Failed to kill process {pid}: {e}", file=sys.stderr)
        return False


def send_sync(file_path: str, port: int | None = None,
              session_path: Path | None = None, config: str = "Debug") -> str | None:
    """Send a file sync command to the engine's HTTP server."""
    if port is None:
        session = load_session(session_path, config)
        port = session.get("port", DEFAULT_PORT) if session else DEFAULT_PORT

    url = f"http://127.0.0.1:{port}/?file={file_path}"

    try:
        with urllib.request.urlopen(url, timeout=5) as response:
            result = response.read().decode("utf-8")
            print(f"Sync response: {result}")
            return result
    except urllib.error.URLError as e:
        print(f"Failed to connect to engine: {e}", file=sys.stderr)
        return None


def ping(port: int | None = None,
         session_path: Path | None = None, config: str = "Debug") -> bool:
    """Ping the engine to check if it's alive."""
    if port is None:
        session = load_session(session_path, config)
        port = session.get("port", DEFAULT_PORT) if session else DEFAULT_PORT

    url = f"http://127.0.0.1:{port}/?ping"

    try:
        with urllib.request.urlopen(url, timeout=2) as response:
            result = response.read().decode("utf-8")
            print(f"Engine alive: {result}")
            return True
    except urllib.error.URLError:
        print("Engine not responding")
        return False


def main():
    parser = argparse.ArgumentParser(description="Engine control utilities")
    parser.add_argument("--config", "-c", default="Debug",
                        help="Build config (Debug/Release)")
    parser.add_argument("--session", "-s", type=Path,
                        help="Path to session.json (auto-detected if not specified)")
    parser.add_argument("--port", "-p", type=int,
                        help="Override HTTP port")

    subparsers = parser.add_subparsers(dest="command", required=True)

    # kill command
    subparsers.add_parser("kill", help="Kill running engine instance")

    # ping command
    subparsers.add_parser("ping", help="Check if engine is alive")

    # sync command
    sync_parser = subparsers.add_parser("sync", help="Sync a file to engine")
    sync_parser.add_argument("file", help="File path to sync")

    # status command
    subparsers.add_parser("status", help="Show engine session info")

    args = parser.parse_args()

    if args.command == "kill":
        success = kill_engine(args.session, args.config)
        sys.exit(0 if success else 1)

    elif args.command == "ping":
        success = ping(args.port, args.session, args.config)
        sys.exit(0 if success else 1)

    elif args.command == "sync":
        result = send_sync(args.file, args.port, args.session, args.config)
        sys.exit(0 if result is not None else 1)

    elif args.command == "status":
        session = load_session(args.session, args.config)
        if session:
            print(json.dumps(session, indent=2))
        else:
            print("No session found")
            sys.exit(1)


if __name__ == "__main__":
    main()

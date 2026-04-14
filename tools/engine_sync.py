#!/usr/bin/env python3
"""Sync a file to the running engine."""
import sys
from engine_control import send_sync

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: engine_sync.py <file_path> [config]", file=sys.stderr)
        sys.exit(1)

    file_path = sys.argv[1]
    config = sys.argv[2] if len(sys.argv) > 2 else "Debug"
    result = send_sync(file_path, config=config)
    sys.exit(0 if result is not None else 1)

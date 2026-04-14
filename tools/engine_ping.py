#!/usr/bin/env python3
"""Ping the engine to check if it's alive."""
import sys
from engine_control import ping

if __name__ == "__main__":
    config = sys.argv[1] if len(sys.argv) > 1 else "Debug"
    success = ping(config=config)
    sys.exit(0 if success else 1)

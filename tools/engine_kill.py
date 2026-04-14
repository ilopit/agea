#!/usr/bin/env python3
"""Kill the running engine instance."""
import sys
from engine_control import kill_engine

if __name__ == "__main__":
    config = sys.argv[1] if len(sys.argv) > 1 else "Debug"
    success = kill_engine(config=config)
    sys.exit(0 if success else 1)

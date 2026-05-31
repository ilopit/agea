"""Hammer model.level.load in a tight loop to flush out the streaming-pipeline
race that crashes the editor during mass create/destroy (level reload).

Usage: launch a kryga_editor (it writes build/project_Debug/tmp/editor_rpc.json),
then run this script. It reloads the level N times and reports the iteration at
which the RPC connection drops (engine crash)."""
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests" / "e2e"))

from rpc_client import EngineRPC  # noqa: E402

LEVEL = "simple_test"
ITERS = int(sys.argv[1]) if len(sys.argv) > 1 else 200


def main():
    rpc = EngineRPC(timeout=10.0)
    rpc.connect()
    rpc.call("ping")
    print(f"connected; hammering {ITERS} reloads of '{LEVEL}'", flush=True)
    t0 = time.time()
    for i in range(ITERS):
        try:
            rpc.call("model.level.load", {"id": LEVEL})
            rpc.wait_frame(2)
        except Exception as exc:  # noqa: BLE001
            print(f"CRASH at iter {i}: {type(exc).__name__}: {exc}", flush=True)
            print(f"survived {i} reloads in {time.time()-t0:.1f}s", flush=True)
            return 1
        if i % 10 == 0:
            print(f"  iter {i} ok", flush=True)
    print(f"completed {ITERS} reloads in {time.time()-t0:.1f}s — no crash", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

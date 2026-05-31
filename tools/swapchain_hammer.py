"""Hammer swapchain recreation (render.config.set frames_in_flight) through the
Phase 4 config-barrier path, optionally interleaved with level reloads, to flush
out the streaming-pipeline crash. Reports the iteration where the engine dies."""
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests" / "e2e"))

from rpc_client import EngineRPC  # noqa: E402

LEVEL = "simple_test"
ITERS = int(sys.argv[1]) if len(sys.argv) > 1 else 200
MODE = sys.argv[2] if len(sys.argv) > 2 else "swap"  # swap | mixed


def main():
    rpc = EngineRPC(timeout=10.0)
    rpc.connect()
    rpc.call("ping")
    print(f"connected; {ITERS} iters mode={MODE}", flush=True)
    t0 = time.time()
    for i in range(ITERS):
        fif = 4 if (i % 2 == 0) else 2
        try:
            rpc.call("render.config.set", {"present_mode": "fifo", "frames_in_flight": fif})
            rpc.wait_frame(2)
            if MODE == "mixed" and i % 3 == 0:
                rpc.call("render.config.set", {"render_scale": {"enabled": (i % 6 == 0), "divisor": 3}})
                rpc.wait_frame(2)
            if MODE == "mixed" and i % 5 == 0:
                rpc.call("model.level.load", {"id": LEVEL})
                rpc.wait_frame(2)
        except Exception as exc:  # noqa: BLE001
            print(f"CRASH at iter {i} (fif={fif}): {type(exc).__name__}: {exc}", flush=True)
            print(f"survived {i} iters in {time.time()-t0:.1f}s", flush=True)
            return 1
        if i % 10 == 0:
            print(f"  iter {i} ok (fif={fif})", flush=True)
    print(f"completed {ITERS} iters in {time.time()-t0:.1f}s — no crash", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

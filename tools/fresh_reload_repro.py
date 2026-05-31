"""Reproduce the fresh-editor + immediate-reload crash: poll-connect the instant
the RPC port opens (no settle), then run the e2e autouse-fixture sequence
(load_level_and_wait + editor.camera.set + wait_frame) in a tight loop."""
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests" / "e2e"))
from rpc_client import EngineRPC  # noqa: E402

TEST_CAMERA = {"position": [0.0, 8.0, 15.0], "pitch": -25.0, "yaw": 0.0}
LEVEL = "simple_test"


def main():
    rpc = EngineRPC(timeout=10.0)
    t0 = time.time()
    while time.time() - t0 < 90:
        try:
            rpc.connect()
            rpc.call("ping")
            break
        except Exception:
            time.sleep(0.03)
    else:
        print("never connected", flush=True)
        return 1
    print(f"connected at +{time.time()-t0:.2f}s — firing reloads immediately", flush=True)
    for i in range(20):
        try:
            rpc.load_level_and_wait(LEVEL, settle_time=0.0, timeout=15.0)
            rpc.call("editor.camera.set", TEST_CAMERA)
            rpc.wait_frame(1)
            print(f"iter {i} ok", flush=True)
        except Exception as exc:  # noqa: BLE001
            print(f"CRASH at iter {i}: {type(exc).__name__}: {exc}", flush=True)
            return 1
    print("no crash", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

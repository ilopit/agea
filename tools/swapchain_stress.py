#!/usr/bin/env python3
"""Hammer FIFO frames_in_flight grow/shrink reconfigures to force the
intermittent fullscreen 2->3 grow crash deterministically.

For each cycle it sets a fif among the rotation (default 2,3,4,3,2,...) at
present_mode fifo, reads back the device's ACTUAL frames_in_flight (so we can
tell whether the driver honored the grow or clamped it — windowed DWM often
clamps, which is why the grow path is only truly exercised fullscreen), and
pings. A dropped connection / failed ping = the editor crashed; we print the
cycle that killed it and the tail of its log.

Usage:
    python tools/swapchain_stress.py --cycles 400
    python tools/swapchain_stress.py --cycles 400 --seq 2 3 4
    python tools/swapchain_stress.py --log build/project_Debug/tmp/editor.log

Requires the editor running with the dynamic-swapchain reconfigure path.
Launch it fullscreen first to actually exercise the grow:
    KRYGA_FULLSCREEN=1 tools/run.sh kryga_editor   (borderless, independent flip)
    KRYGA_FULLSCREEN=2 tools/run.sh kryga_editor   (exclusive, honors image count)
"""

import argparse
import json
import socket
import sys
import time


def rpc_connect(timeout=15):
    port = json.load(open("build/project_Debug/tmp/editor_rpc.json"))["port"]
    s = socket.create_connection(("127.0.0.1", port))
    s.settimeout(timeout)
    return s


class Rpc:
    def __init__(self, sock):
        self.s = sock
        self.buf = b""
        self.mid = 0

    def call(self, method, params):
        self.mid += 1
        body = json.dumps({"jsonrpc": "2.0", "id": self.mid, "method": method, "params": params})
        self.s.sendall(f"Content-Length: {len(body)}\r\n\r\n{body}".encode())
        while True:  # skip interleaved log notifications until our id
            while b"\r\n\r\n" not in self.buf:
                self.buf += self.s.recv(65536)
            h, _, rest = self.buf.partition(b"\r\n\r\n")
            clen = int([l for l in h.split(b"\r\n")
                        if l.lower().startswith(b"content-length")][0].split(b":")[1])
            while len(rest) < clen:
                rest += self.s.recv(65536)
            body, self.buf = rest[:clen], rest[clen:]
            m = json.loads(body)
            if m.get("id") == self.mid:
                return m


def log_tail(path, n=40):
    try:
        with open(path, "r", errors="replace") as f:
            return "".join(f.readlines()[-n:])
    except OSError:
        return f"(could not read log {path})"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cycles", type=int, default=400, help="reconfigure cycles to run")
    ap.add_argument("--seq", nargs="+", type=int, default=[2, 3, 4, 3],
                    help="frames_in_flight rotation to cycle through")
    ap.add_argument("--mode", default="fifo", help="present mode to hold")
    ap.add_argument("--settle", type=float, default=0.25,
                    help="seconds to wait after each reconfigure before pinging")
    ap.add_argument("--log", default="build/project_Debug/tmp/editor.log",
                    help="editor log to tail on crash")
    args = ap.parse_args()

    rpc = Rpc(rpc_connect())
    # Baseline: confirm alive + record starting device fif. frames_in_flight
    # lives in render.config.get; present_mode in render.stats.
    cfg = rpc.call("render.config.get", {}).get("result", {})
    st = rpc.call("render.stats", {}).get("result", {})
    print(f"baseline: device fif {cfg.get('frames_in_flight')} "
          f"present {st.get('present_mode')}", flush=True)

    last_fif = None
    for c in range(args.cycles):
        want = args.seq[c % len(args.seq)]
        try:
            r = rpc.call("render.config.set",
                         {"present_mode": args.mode, "frames_in_flight": want})
            if "error" in r:
                print(f"[cycle {c}] config.set error: {r['error']}", file=sys.stderr, flush=True)
            time.sleep(args.settle)
            cfg = rpc.call("render.config.get", {}).get("result", {})
            got = cfg.get("frames_in_flight")
            transition = f"{last_fif}->{got}" if last_fif is not None else str(got)
            grow = (last_fif is not None and got is not None and got > last_fif)
            tag = " GROW" if grow else ""
            print(f"[cycle {c}] req fif {want} -> device {got}  ({transition}){tag}", flush=True)
            last_fif = got
            # health check
            if rpc.call("ping", {}).get("result") is None:
                print(f"[cycle {c}] ping returned no result", file=sys.stderr, flush=True)
        except (ConnectionResetError, ConnectionAbortedError, socket.timeout, OSError) as e:
            print(f"\n*** EDITOR DIED at cycle {c} (req fif {want}, "
                  f"prev device fif {last_fif}): {type(e).__name__}: {e}", flush=True)
            print("\n--- editor log tail ---", flush=True)
            print(log_tail(args.log), flush=True)
            return 1

    print(f"\nsurvived {args.cycles} cycles, no crash.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

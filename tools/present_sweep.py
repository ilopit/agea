#!/usr/bin/env python3
"""Sweep present_mode x frames_in_flight and PresentMon-capture each, one table.

The point: test whether windowed FIFO display latency scales with frames_in_flight
(render-ahead depth feeding DWM's flip queue) or is flat (compositor-fixed). It
flips the live engine config over JSON-RPC, lets it stabilize, captures via
PresentMon, and prints avg DisplayLatency + the PresentMode the OS actually used.

Usage (ELEVATED shell — ETW needs admin):
    python tools/present_sweep.py --presentmon "C:\\path\\PresentMon-2.4.1-x64.exe"

    # custom sweep / longer captures
    python tools/present_sweep.py --pm <path> --seconds 8 \\
        --configs fifo:2 fifo:3 fifo:4 mailbox:3 immediate:2

Requires the editor running (reads build/project_Debug/tmp/editor_rpc.json) with
the dynamic-swapchain reconfigure path, so present_mode/frames_in_flight switch
live without a restart.
"""

import argparse
import json
import os
import socket
import sys
import tempfile
import time

import present_latency as pl


def rpc_connect():
    port = json.load(open("build/project_Debug/tmp/editor_rpc.json"))["port"]
    s = socket.create_connection(("127.0.0.1", port))
    s.settimeout(15)
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


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--presentmon", "--pm", dest="presentmon", help="path to PresentMon.exe")
    ap.add_argument("--process", default="kryga_editor.exe")
    ap.add_argument("--seconds", type=int, default=6, help="capture duration per config")
    ap.add_argument("--settle", type=float, default=1.5,
                    help="seconds to let a config stabilize before capturing")
    ap.add_argument("--configs", nargs="+",
                    default=["fifo:2", "fifo:3", "fifo:4", "mailbox:3", "immediate:2"],
                    help="mode:fif pairs, e.g. fifo:2 mailbox:3")
    args = ap.parse_args()

    pm = pl.find_presentmon(args.presentmon)
    if not pm:
        print("PresentMon.exe not found — pass --presentmon <path>.", file=sys.stderr)
        return 2

    rpc = Rpc(rpc_connect())
    results = []
    for cfg in args.configs:
        mode, _, fif = cfg.partition(":")
        fif = int(fif)
        r = rpc.call("render.config.set", {"present_mode": mode, "frames_in_flight": fif})
        if "error" in r:
            print(f"[{cfg}] config.set error: {r['error']}", file=sys.stderr)
            continue
        # Read back what the device actually resolved (mode may fall back, fif clamps).
        st = rpc.call("render.stats", {}).get("result", {})
        print(f"\n=== {cfg}  (device: {st.get('present_mode')} / requested fif {fif}) ===",
              flush=True)
        time.sleep(args.settle)
        out = os.path.join(tempfile.gettempdir(), f"kryga_pm_{mode}_{fif}.csv")
        if os.path.exists(out):
            os.remove(out)
        if not pl.capture(pm, args.process, args.seconds, out):
            print(f"[{cfg}] capture failed", file=sys.stderr)
            continue
        data = pl.collect(out)
        if not data:
            print(f"[{cfg}] no rows captured", file=sys.stderr)
            continue
        results.append((cfg, st.get("present_mode"), st.get("present_latency_ms"), data))

    # Comparison table.
    print("\n" + "=" * 78)
    print("PRESENT SWEEP  (display latency = present->scanout, incl. compositor)")
    print(f"{'config':<14}{'device':<11}{'fps':>6}{'frame ms':>10}"
          f"{'disp avg':>10}{'disp p99':>10}{'gpu avg':>9}{'in-eng':>8}")
    print("-" * 78)
    for cfg, dev, eng_ms, d in results:
        fr = d["frame"]; ds = d["disp"]; gp = d["gpu"]
        fps = 1000.0 / fr["avg"] if fr else 0
        print(f"{cfg:<14}{(dev or '?'):<11}{fps:>6.1f}"
              f"{(fr['avg'] if fr else 0):>10.2f}"
              f"{(ds['avg'] if ds else 0):>10.2f}"
              f"{(ds['p99'] if ds else 0):>10.2f}"
              f"{(gp['avg'] if gp else 0):>9.2f}"
              f"{(eng_ms or 0):>8.2f}")
    print("-" * 78)
    for cfg, dev, eng_ms, d in results:
        modes = ", ".join(f"{k} ({v})" for k, v in
                          sorted(d["modes"].items(), key=lambda kv: -kv[1])) or "n/a"
        print(f"  {cfg:<12} PresentMode: {modes}")
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())

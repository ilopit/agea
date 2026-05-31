#!/usr/bin/env python3
"""Measure present-wait pacing: hold FIFO, sweep present_pace_frames, PresentMon-
capture each. Proves the render-ahead queue collapse (display latency drop).

Run fullscreen (independent flip) for an honest metric:
    KRYGA_FULLSCREEN=1 tools/run.sh kryga_editor   (separate launch)
    python tools/pace_sweep.py --pm <PresentMon.exe> --configs 3:0 3:2 3:1 2:0 2:1

Each config is fif:pace (frames_in_flight : present_pace_frames), present_mode
fifo. Requires the editor running with the present-pacing build.
"""
import argparse, json, os, socket, sys, tempfile, time
import present_latency as pl


def rpc_connect():
    port = json.load(open("build/project_Debug/tmp/editor_rpc.json"))["port"]
    s = socket.create_connection(("127.0.0.1", port)); s.settimeout(20)
    return s


class Rpc:
    def __init__(self, sock):
        self.s = sock; self.buf = b""; self.mid = 0
    def call(self, method, params):
        self.mid += 1
        body = json.dumps({"jsonrpc": "2.0", "id": self.mid, "method": method, "params": params})
        self.s.sendall(f"Content-Length: {len(body)}\r\n\r\n{body}".encode())
        while True:
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
    ap.add_argument("--presentmon", "--pm", dest="presentmon")
    ap.add_argument("--process", default="kryga_editor.exe")
    ap.add_argument("--seconds", type=int, default=8)
    ap.add_argument("--settle", type=float, default=2.5)
    ap.add_argument("--configs", nargs="+", default=["3:0", "3:2", "3:1", "2:0", "2:1"],
                    help="fif:pace pairs, present_mode held at fifo")
    args = ap.parse_args()

    pm = pl.find_presentmon(args.presentmon)
    if not pm:
        print("PresentMon.exe not found — pass --presentmon <path>.", file=sys.stderr)
        return 2

    rpc = Rpc(rpc_connect())
    results = []
    for cfg in args.configs:
        fif, _, pace = cfg.partition(":")
        fif, pace = int(fif), int(pace)
        r = rpc.call("render.config.set",
                     {"present_mode": "fifo", "frames_in_flight": fif, "present_pace_frames": pace})
        if "error" in r:
            print(f"[{cfg}] config.set error: {r['error']}", file=sys.stderr); continue
        c = rpc.call("render.config.get", {}).get("result", {})
        st = rpc.call("render.stats", {}).get("result", {})
        print(f"\n=== fif {fif} pace {pace}  (device fif {c.get('frames_in_flight')} "
              f"pace {c.get('present_pace_frames')} present {st.get('present_mode')}) ===",
              flush=True)
        time.sleep(args.settle)
        out = os.path.join(tempfile.gettempdir(), f"kryga_pace_{fif}_{pace}.csv")
        if os.path.exists(out):
            os.remove(out)
        if not pl.capture(pm, args.process, args.seconds, out):
            print(f"[{cfg}] capture failed", file=sys.stderr); continue
        data = pl.collect(out)
        if not data:
            print(f"[{cfg}] no rows", file=sys.stderr); continue
        results.append((fif, pace, c.get("present_pace_frames"), st.get("present_mode"),
                        st.get("present_latency_ms"), data))

    print("\n" + "=" * 78)
    print("PACE SWEEP  (fifo; display latency = present->scanout)")
    print(f"{'fif':>4}{'pace':>6}{'device':<12}{'fps':>7}{'frame':>8}"
          f"{'disp avg':>10}{'disp p99':>10}{'in-eng':>8}")
    print("-" * 78)
    for fif, pace, dpace, dev, eng, d in results:
        fr = d["frame"]; ds = d["disp"]
        fps = 1000.0 / fr["avg"] if fr else 0
        print(f"{fif:>4}{dpace:>6}{(dev or '?'):<12}{fps:>7.1f}"
              f"{(fr['avg'] if fr else 0):>8.2f}"
              f"{(ds['avg'] if ds else 0):>10.2f}"
              f"{(ds['p99'] if ds else 0):>10.2f}"
              f"{(eng or 0):>8.2f}")
    print("-" * 78)
    return 0


if __name__ == "__main__":
    sys.exit(main())

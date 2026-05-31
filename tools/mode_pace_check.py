#!/usr/bin/env python3
"""Check whether present-wait pacing throttles non-FIFO modes. Sweeps
mode:fif:pace and PresentMon-captures each. If immediate/mailbox fps or display
latency change between pace 0 and pace 2, pacing is (wrongly) affecting them."""
import argparse, json, os, socket, sys, tempfile, time
import present_latency as pl


class Rpc:
    def __init__(self):
        port = json.load(open("build/project_Debug/tmp/editor_rpc.json"))["port"]
        self.s = socket.create_connection(("127.0.0.1", port)); self.s.settimeout(20)
        self.buf = b""; self.mid = 0
    def call(self, method, params):
        self.mid += 1
        b = json.dumps({"jsonrpc": "2.0", "id": self.mid, "method": method, "params": params})
        self.s.sendall(f"Content-Length: {len(b)}\r\n\r\n{b}".encode())
        while True:
            while b"\r\n\r\n" not in self.buf: self.buf += self.s.recv(65536)
            h, _, rest = self.buf.partition(b"\r\n\r\n")
            clen = int([l for l in h.split(b"\r\n")
                        if l.lower().startswith(b"content-length")][0].split(b":")[1])
            while len(rest) < clen: rest += self.s.recv(65536)
            body, self.buf = rest[:clen], rest[clen:]
            m = json.loads(body)
            if m.get("id") == self.mid: return m


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--presentmon", "--pm", dest="presentmon")
    ap.add_argument("--seconds", type=int, default=8)
    ap.add_argument("--settle", type=float, default=2.5)
    # mode:fif:pace
    ap.add_argument("--configs", nargs="+",
                    default=["immediate:2:0", "immediate:2:2", "mailbox:3:0", "mailbox:3:2"])
    args = ap.parse_args()
    pm = pl.find_presentmon(args.presentmon)
    if not pm:
        print("PresentMon not found", file=sys.stderr); return 2
    rpc = Rpc(); rows = []
    for cfg in args.configs:
        mode, fif, pace = cfg.split(":"); fif, pace = int(fif), int(pace)
        rpc.call("render.config.set",
                 {"present_mode": mode, "frames_in_flight": fif, "present_pace_frames": pace})
        st = rpc.call("render.stats", {}).get("result", {})
        c = rpc.call("render.config.get", {}).get("result", {})
        print(f"\n=== {mode} fif{fif} pace{pace}  (device {st.get('present_mode')} "
              f"pace {c.get('present_pace_frames')}) ===", flush=True)
        time.sleep(args.settle)
        out = os.path.join(tempfile.gettempdir(), f"kryga_mp_{mode}_{fif}_{pace}.csv")
        if os.path.exists(out): os.remove(out)
        if not pl.capture(pm, "kryga_editor.exe", args.seconds, out): continue
        d = pl.collect(out)
        if d: rows.append((mode, fif, pace, st.get("present_mode"), d))
    print("\n" + "=" * 66)
    print(f"{'set mode':<11}{'pace':>5}{'device':<11}{'fps':>8}{'frame':>8}{'disp avg':>10}")
    print("-" * 66)
    for mode, fif, pace, dev, d in rows:
        fr = d["frame"]; ds = d["disp"]; fps = 1000.0 / fr["avg"] if fr else 0
        print(f"{mode:<11}{pace:>5}{(dev or '?'):<11}{fps:>8.1f}"
              f"{(fr['avg'] if fr else 0):>8.2f}{(ds['avg'] if ds else 0):>10.2f}")
    print("-" * 66)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Hammer fullscreen swapchain recreates (present-mode changes + image-count
grows/shrinks) to force the intermittent fullscreen reconfigure crash. Detects
death via connection drop / failed ping and reports the transition that killed
it. Run the editor fullscreen (KRYGA_FULLSCREEN=1) first; WER LocalDumps should
be configured so a full dump lands in build/crashdumps."""
import argparse, json, socket, sys, time


def connect():
    port = json.load(open("build/project_Debug/tmp/editor_rpc.json"))["port"]
    s = socket.create_connection(("127.0.0.1", port)); s.settimeout(20)
    return s


class Rpc:
    def __init__(self): self.s = connect(); self.buf = b""; self.mid = 0
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
    ap.add_argument("--cycles", type=int, default=600)
    ap.add_argument("--settle", type=float, default=0.12)
    # Mix of mode changes and image-count grows/shrinks. mailbox forces >=3.
    ap.add_argument("--seq", nargs="+",
                    default=["mailbox:4", "immediate:2", "fifo:3", "immediate:2",
                             "mailbox:3", "fifo:2", "mailbox:4", "fifo:2"])
    args = ap.parse_args()
    rpc = Rpc()
    last = None
    for c in range(args.cycles):
        cfg = args.seq[c % len(args.seq)]
        mode, _, fif = cfg.partition(":"); fif = int(fif)
        try:
            r = rpc.call("render.config.set", {"present_mode": mode, "frames_in_flight": fif})
            if "error" in r:
                print(f"[{c}] set err {r['error']}", file=sys.stderr, flush=True)
            time.sleep(args.settle)
            st = rpc.call("render.stats", {}).get("result", {})
            cg = rpc.call("render.config.get", {}).get("result", {})
            got = cg.get("frames_in_flight")
            if c % 16 == 0:
                print(f"[{c}] {last} -> {mode}:{got} (dev {st.get('present_mode')})", flush=True)
            last = f"{mode}:{got}"
            rpc.call("ping", {})
        except (ConnectionResetError, ConnectionAbortedError, socket.timeout, OSError) as e:
            print(f"\n*** EDITOR DIED at cycle {c}: set {cfg}, prev {last}: "
                  f"{type(e).__name__}: {e}", flush=True)
            return 1
    print(f"\nsurvived {args.cycles} cycles, no crash.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

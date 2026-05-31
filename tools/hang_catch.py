#!/usr/bin/env python3
"""Reproduce the fullscreen reconfigure HANG and snapshot native thread stacks
the instant it happens. Drives long-settle present reconfigures; after each it
pings with a short timeout. On the first unresponsive ping the editor is hung,
so it attaches cdb (local symbols, NO .reload -> avoids the network-symbol hang)
to dump every thread's stack, which reveals where the render/main thread is
parked. Run the editor fullscreen first.
"""
import argparse, json, os, socket, subprocess, sys, time

CDB = r"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
SYM = r"F:\dev\kryga\1\build\bin\Debug"


def rpc_port_pid():
    j = json.load(open("build/project_Debug/tmp/editor_rpc.json"))
    return j["port"], j["pid"]


class Rpc:
    def __init__(self, port, timeout):
        self.s = socket.create_connection(("127.0.0.1", port)); self.s.settimeout(timeout)
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


def dump_stacks(pid, out_path):
    print(f"\n*** attaching cdb to hung pid {pid} ***", flush=True)
    env = dict(os.environ, _NT_SYMBOL_PATH=SYM)
    try:
        r = subprocess.run([CDB, "-y", SYM, "-p", str(pid),
                            "-c", "~*kn 40;qd"],
                           capture_output=True, text=True, timeout=90, env=env)
        with open(out_path, "w", errors="replace") as f:
            f.write(r.stdout); f.write("\n--- stderr ---\n"); f.write(r.stderr)
        print(f"stacks written to {out_path} (cdb exit {r.returncode})", flush=True)
    except subprocess.TimeoutExpired:
        print("cdb timed out", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--settle", type=float, default=9.0)
    ap.add_argument("--cycles", type=int, default=30)
    ap.add_argument("--ping-timeout", type=float, default=4.0)
    ap.add_argument("--out", default="build/crashdumps/hang_stacks.txt")
    ap.add_argument("--seq", nargs="+",
                    default=["mailbox:4", "immediate:2", "fifo:3", "fifo:2"])
    args = ap.parse_args()
    port, pid = rpc_port_pid()
    print(f"editor port {port} pid {pid}", flush=True)
    drv = Rpc(port, 6.0)
    for c in range(args.cycles):
        cfg = args.seq[c % len(args.seq)]
        mode, _, fif = cfg.partition(":"); fif = int(fif)
        try:
            drv.call("render.config.set", {"present_mode": mode, "frames_in_flight": fif})
        except (socket.timeout, OSError) as e:
            print(f"[{c}] set {cfg} timed out -> HANG: {type(e).__name__}", flush=True)
            dump_stacks(pid, args.out); return 0
        time.sleep(args.settle)
        # health probe on a SEPARATE short-timeout connection
        try:
            probe = Rpc(port, args.ping_timeout)
            probe.call("ping", {})
            print(f"[{c}] {cfg}: alive", flush=True)
            probe.s.close()
        except (socket.timeout, OSError) as e:
            print(f"[{c}] ping after {cfg} timed out -> HANG: {type(e).__name__}", flush=True)
            dump_stacks(pid, args.out); return 0
    print(f"\nsurvived {args.cycles} cycles, no hang.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

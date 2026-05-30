import json, socket, time, base64, sys, hashlib

with open("build/project_Debug/tmp/editor_rpc.json") as f:
    port = json.load(f)["port"]

s = socket.create_connection(("127.0.0.1", port))
s.settimeout(15)
buf = b""

def send(method, params, mid):
    body = json.dumps({"jsonrpc": "2.0", "id": mid, "method": method, "params": params})
    s.sendall(f"Content-Length: {len(body)}\r\n\r\n{body}".encode())

def recv_one():
    global buf
    while b"\r\n\r\n" not in buf:
        buf += s.recv(65536)
    header, _, rest = buf.partition(b"\r\n\r\n")
    clen = 0
    for line in header.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            clen = int(line.split(b":")[1])
    while len(rest) < clen:
        rest += s.recv(65536)
    body, buf = rest[:clen], rest[clen:]
    return json.loads(body)

mid = 0
def call(method, params):
    global mid
    mid += 1
    send(method, params, mid)
    # Server interleaves log notifications (no/null id). Skip until our id.
    while True:
        msg = recv_one()
        if msg.get("id") == mid:
            return msg

def set_mode(m):
    r = call("render.config.set", {"present_mode": m})
    print(f"set present_mode={m}: {r.get('result', r.get('error'))}", flush=True)

def shot():
    r = call("render.screenshot", {"x": 0, "y": 0, "width": 1280, "height": 720})
    res = r.get("result", {})
    b64 = res.get("image", "") if isinstance(res, dict) else ""
    if not b64:
        print(f"    shot err: {r.get('error')}", flush=True)
        return None
    if "," in b64[:64]:           # strip optional data: URI prefix
        b64 = b64.split(",", 1)[1]
    b64 = "".join(b64.split())    # strip all whitespace/newlines
    b64 += "=" * (-len(b64) % 4)  # fix padding
    try:
        return base64.b64decode(b64)
    except Exception as e:
        print(f"    b64 decode fail len={len(b64)}: {e}", flush=True)
        return None

# Diff N consecutive frames on a STATIC scene. If a mode flickers, consecutive
# frames differ a lot; a clean mode yields near-identical frames.
def measure(mode, n=4, gap=0.1):
    set_mode(mode)
    time.sleep(0.6)
    for i in range(n):
        img = shot()
        if img is None:
            print(f"  {mode}[{i}]: NO IMAGE", flush=True)
            time.sleep(gap); continue
        path = f"/tmp/diag_{mode}_{i}.png"
        with open(path, "wb") as f:
            f.write(img)
        print(f"  {mode}[{i}]: {len(img)} bytes -> {path}", flush=True)
        time.sleep(gap)

for m in ["mailbox", "fifo", "immediate"]:
    measure(m)

s.close()
print("done")

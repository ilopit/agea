"""
RPC client for Kryga engine regression tests.

Persistent TCP connection with Content-Length framing (JSON-RPC 2.0).
Adapted from tools/engine_control.py and tools/mcp_scene_server.py.
"""
import json
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Optional

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

UINT32_MAX = 0xFFFFFFFF

def _default_discovery(config: str = "Debug") -> Path:
    return PROJECT_ROOT / "build" / f"project_{config}" / "tmp" / "editor_rpc.json"

DEFAULT_DISCOVERY = _default_discovery()


class EngineProcess:
    """Manages a kryga_editor subprocess."""

    def __init__(
        self,
        config: str = "Debug",
        level: Optional[str] = None,
        discovery_path: Optional[Path] = None,
    ):
        self._proc: Optional[subprocess.Popen] = None
        self._config = config
        self._level = level
        self._discovery = discovery_path or DEFAULT_DISCOVERY

    @property
    def discovery(self) -> Path:
        return self._discovery

    @staticmethod
    def _is_pid_running(pid: int) -> bool:
        if sys.platform == "win32":
            import ctypes
            kernel32 = ctypes.windll.kernel32
            PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
            handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
            if handle:
                kernel32.CloseHandle(handle)
                return True
            return False
        try:
            os.kill(pid, 0)
            return True
        except (OSError, ProcessLookupError):
            return False

    @staticmethod
    def is_engine_alive(discovery_path: Optional[Path] = None) -> bool:
        dp = discovery_path or DEFAULT_DISCOVERY
        if not dp.exists():
            return False
        try:
            info = json.loads(dp.read_text())
            pid = info.get("pid")
            if pid is None:
                return False
            return EngineProcess._is_pid_running(pid)
        except (json.JSONDecodeError, OSError):
            return False

    @staticmethod
    def clean_stale_discovery(discovery_path: Optional[Path] = None):
        dp = discovery_path or DEFAULT_DISCOVERY
        if dp.exists() and not EngineProcess.is_engine_alive(dp):
            dp.unlink(missing_ok=True)

    def _exe_path(self) -> Path:
        return PROJECT_ROOT / "build" / f"project_{self._config}" / "bin" / "kryga_editor.exe"

    def start(self, startup_timeout: float = 15.0) -> None:
        exe = self._exe_path()
        if not exe.exists():
            raise FileNotFoundError(f"Engine executable not found: {exe}")

        dp = self._discovery
        dp.parent.mkdir(parents=True, exist_ok=True)
        dp.unlink(missing_ok=True)

        self._log_path = dp.with_suffix(".log")
        self._log_file = open(self._log_path, "w")

        cmd = [str(exe)]
        if self._level:
            cmd += ["--level", self._level]
        if self._discovery != DEFAULT_DISCOVERY:
            cmd += ["--discovery", str(self._discovery)]

        self._proc = subprocess.Popen(
            cmd,
            cwd=str(exe.parent),
            stdout=self._log_file,
            stderr=subprocess.STDOUT,
        )

        deadline = time.monotonic() + startup_timeout
        while time.monotonic() < deadline:
            if self._proc.poll() is not None:
                raise RuntimeError(
                    f"Engine exited immediately with code {self._proc.returncode}"
                )
            if dp.exists():
                try:
                    info = json.loads(dp.read_text())
                    if "port" in info:
                        return
                except (json.JSONDecodeError, OSError):
                    pass
            time.sleep(0.3)
        self.stop()
        raise TimeoutError(
            f"Engine did not write discovery file within {startup_timeout}s"
        )

    def stop(self) -> None:
        if self._proc is None:
            return
        try:
            if sys.platform == "win32":
                self._proc.terminate()
            else:
                self._proc.send_signal(signal.SIGTERM)
            self._proc.wait(timeout=5)
        except (subprocess.TimeoutExpired, OSError):
            self._proc.kill()
            try:
                self._proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass
        self._proc = None
        if hasattr(self, "_log_file") and self._log_file:
            self._log_file.close()
            self._log_file = None

    @property
    def running(self) -> bool:
        return self._proc is not None and self._proc.poll() is None


class EngineRPC:

    def __init__(self, timeout: float = 10.0, discovery_path: Optional[Path] = None):
        self._sock: Optional[socket.socket] = None
        self._buf = b""
        self._next_id = 1
        self._timeout = timeout
        self._discovery = discovery_path or DEFAULT_DISCOVERY

    @property
    def discovery(self) -> Path:
        return self._discovery

    def connect(self) -> None:
        p = self._discovery
        if not p.exists():
            raise ConnectionError(f"Engine not running (no {p})")
        info = json.loads(p.read_text())
        self._sock = socket.create_connection(
            ("127.0.0.1", info["port"]), timeout=self._timeout
        )
        self._sock.settimeout(self._timeout)
        self._buf = b""

    @property
    def connected(self) -> bool:
        return self._sock is not None

    def _send(self, msg: dict) -> None:
        body = json.dumps(msg).encode("utf-8")
        self._sock.sendall(
            f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body
        )

    def _recv_response(self, want_id: int) -> dict:
        while True:
            while b"\r\n\r\n" not in self._buf:
                chunk = self._sock.recv(8192)
                if not chunk:
                    self._sock = None
                    raise ConnectionError("Engine closed connection")
                self._buf += chunk
            head, _, rest = self._buf.partition(b"\r\n\r\n")
            length = 0
            for line in head.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1].strip())
            while len(rest) < length:
                chunk = self._sock.recv(length - len(rest))
                if not chunk:
                    self._sock = None
                    raise ConnectionError("Engine closed mid-frame")
                rest += chunk
            msg = json.loads(rest[:length].decode("utf-8"))
            self._buf = rest[length:]
            if msg.get("id") == want_id:
                return msg

    def call(self, method: str, params: Optional[dict] = None) -> Any:
        if not self._sock:
            self.connect()
        req_id = self._next_id
        self._next_id += 1
        try:
            self._send(
                {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "method": method,
                    "params": params or {},
                }
            )
            resp = self._recv_response(req_id)
        except (ConnectionError, OSError, socket.timeout):
            self._sock = None
            raise
        if "error" in resp:
            raise RuntimeError(resp["error"].get("message", str(resp["error"])))
        return resp.get("result")

    def disconnect(self) -> None:
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    def wait_frame(self, count: int = 1) -> None:
        """Block until the engine completes `count` full frame cycles.

        After this returns, any model mutations made before the call are
        guaranteed to be reflected in the render cache (consume_updated_render
        has run).
        """
        self.call("engine.waitFrame", {"count": count})

    def call_queued(
        self,
        method: str,
        params: Optional[dict] = None,
        settle_time: float = 0.5,
        poll_timeout: float = 5.0,
    ) -> Any:
        """Call an async RPC ({queued:true}), then poll ping to confirm completion."""
        result = self.call(method, params)
        if not (isinstance(result, dict) and result.get("queued")):
            return result
        time.sleep(settle_time)
        deadline = time.monotonic() + poll_timeout
        while time.monotonic() < deadline:
            try:
                self.call("ping")
                return result
            except (ConnectionError, OSError, socket.timeout):
                time.sleep(0.2)
        raise TimeoutError(
            f"{method} queued but engine did not respond within {poll_timeout}s"
        )

    def invoke(self, obj_id: str, function: str, args: Optional[list] = None) -> Any:
        """Invoke a reflected function on an object. Returns the result value."""
        params = {"id": obj_id, "function": function}
        if args is not None:
            params["args"] = args
        result = self.call("model.object.function.invoke", params)
        return result.get("value") if isinstance(result, dict) else result

    def get_type_meta(self, type_name: str) -> dict:
        """Get full type metadata including properties and functions."""
        return self.call("model.type.meta", {"type": type_name})

    def load_level_and_wait(
        self, level_id: str, settle_time: float = 1.0, timeout: float = 10.0
    ) -> dict:
        """Load a level and poll model.scene.getRoot until it's fully loaded."""
        self.call("model.level.load", {"id": level_id})
        time.sleep(settle_time)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                root = self.call("model.scene.getRoot")
                if root.get("level") == level_id and root.get("children"):
                    return root
            except (ConnectionError, OSError, socket.timeout, RuntimeError):
                pass
            time.sleep(0.3)
        raise TimeoutError(
            f"model.level.load('{level_id}') did not complete within {timeout}s"
        )

    def screenshot(self, use_selection: bool = False,
                   x: int = 0, y: int = 0,
                   width: int = 0, height: int = 0) -> str:
        """Capture viewport screenshot. Returns the absolute path to the saved PNG."""
        params = {}
        if use_selection:
            params["use_selection"] = True
        elif x or y or width or height:
            params = {"x": x, "y": y, "width": width, "height": height}
        result = self.call("render.screenshot", params)
        return result["path"]

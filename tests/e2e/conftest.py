"""
Shared fixtures for Kryga engine regression tests.

Auto-starts kryga_editor if not already running.
Use --level to override the default test level.
Use --engine-config to select build config (Debug/Release).

Supports pytest-xdist: each worker gets its own engine instance with a
unique discovery file (build/project_Debug/tmp/editor_rpc_gw0.json, etc.).
"""
import sys
import time
from pathlib import Path
import pytest
from .rpc_client import EngineProcess, EngineRPC, PROJECT_ROOT, _default_discovery

sys.path.insert(0, str(PROJECT_ROOT / "build" / "kryga_generated" / "python"))

SIMPLE_TEST_LEVEL = "simple_test"

EXPECTED_SCENE_OBJECTS = {"hero_cube", "ground", "sun"}

EXPECTED_RENDER_OBJECTS = {
    "hero_cube_mesh": {"mesh": "cube_mesh", "material": "mt_toon"},
    "ground_mesh": {"mesh": "cube_mesh", "material": "mt_toon"},
}

SNAPSHOT_DIRS = [
    "resources/levels/{level}.alvl",
    "resources/packages/base.apkg/class/materials",
]

_engine_process: EngineProcess | None = None
_file_snapshot: dict[Path, bytes] = {}


def _worker_discovery_path(worker_id: str, config: str = "Debug") -> Path:
    """Per-worker discovery file. 'master' (no xdist) uses the default path."""
    base = _default_discovery(config)
    if worker_id == "master":
        return base
    return base.parent / f"editor_rpc_{worker_id}.json"


def _snapshot_files(level: str) -> dict[Path, bytes]:
    snapshot = {}
    for pattern in SNAPSHOT_DIRS:
        d = PROJECT_ROOT / pattern.format(level=level)
        if d.is_dir():
            for f in d.rglob("*"):
                if f.is_file():
                    snapshot[f] = f.read_bytes()
    return snapshot


def _restore_files(snapshot: dict[Path, bytes]) -> None:
    for path, content in snapshot.items():
        if path.read_bytes() != content:
            path.write_bytes(content)


def pytest_addoption(parser):
    parser.addoption(
        "--level",
        default=SIMPLE_TEST_LEVEL,
        help=f"Level to load for tests (default: {SIMPLE_TEST_LEVEL})",
    )
    parser.addoption(
        "--engine-config",
        default="Debug",
        help="Build configuration to use when auto-starting engine (default: Debug)",
    )
    parser.addoption(
        "--no-auto-start",
        action="store_true",
        default=False,
        help="Skip auto-starting the engine (fail if not already running)",
    )
    parser.addoption(
        "--slow",
        type=float,
        default=0,
        help="Pause N seconds between tests for visual inspection (default: 0, try 5)",
    )


@pytest.fixture(scope="session")
def engine(request):
    global _engine_process, _file_snapshot

    level = request.config.getoption("--level")
    config = request.config.getoption("--engine-config")
    no_auto = request.config.getoption("--no-auto-start")

    worker_id = getattr(request.config, "workerinput", {}).get("workerid", "master")
    disco_path = _worker_discovery_path(worker_id, config)

    _file_snapshot = _snapshot_files(level)

    rpc = EngineRPC(timeout=10.0, discovery_path=disco_path)
    connected = False

    if worker_id == "master":
        alive = EngineProcess.is_engine_alive(disco_path)
        if alive:
            try:
                rpc.connect()
                rpc.call("ping")
                connected = True
                print(f"\n[engine] Connected to existing engine")
            except (ConnectionError, OSError, RuntimeError) as exc:
                print(f"\n[engine] Existing engine failed: {exc}")
                rpc.disconnect()

    if not connected:
        EngineProcess.clean_stale_discovery(disco_path)

        if no_auto:
            pytest.skip("Engine not reachable and --no-auto-start specified")

        print(f"\n[engine:{worker_id}] Auto-starting (config={config}, level={level})")
        _engine_process = EngineProcess(
            config=config, level=level, discovery_path=disco_path,
        )
        try:
            _engine_process.start(startup_timeout=20.0)
        except (FileNotFoundError, RuntimeError, TimeoutError) as exc:
            pytest.skip(f"Could not start engine: {exc}")

        print(f"[engine:{worker_id}] Started PID={_engine_process._proc.pid}")
        rpc.connect()
        rpc.call("ping")
        rpc.call("model.scene.getRoot")
        print(f"[engine:{worker_id}] Ping OK, initial scene ready")

    yield rpc

    rpc.disconnect()
    if _engine_process is not None:
        _engine_process.stop()
        _engine_process = None


TEST_CAMERA = {"position": [0.0, 8.0, 15.0], "pitch": -25.0, "yaw": 0.0}


@pytest.fixture
def slow(request, engine):
    delay = request.config.getoption("--slow")
    def _pause(label=""):
        if delay > 0:
            engine.wait_frame()
            msg = f"  [slow] {label} — " if label else "  [slow] "
            print(f"\n{msg}waiting {delay}s")
            time.sleep(delay)
    return _pause


@pytest.fixture(autouse=True)
def load_test_level(engine, request):
    level = request.config.getoption("--level")
    slow = request.config.getoption("--slow")
    root = engine.load_level_and_wait(level, settle_time=1.0, timeout=15.0)
    engine.call("editor.camera.set", TEST_CAMERA)
    engine.wait_frame()
    if slow > 0:
        print(f"\n  [slow] setup ready — waiting {slow}s")
        time.sleep(slow)
    yield root
    if slow > 0:
        engine.wait_frame()
        print(f"\n  [slow] test done — waiting {slow}s")
        time.sleep(slow)
    _restore_files(_file_snapshot)

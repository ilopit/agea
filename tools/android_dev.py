#!/usr/bin/env python3
"""
Kryga Android dev helper.

Wraps the build/install/launch/logcat cycle so one permission pattern
(`Bash(python tools/android_dev.py:*)`) covers everything.

Assumes Windows host + Git Bash / Python 3.8+. Uses $LOCALAPPDATA as the
Android SDK parent. Override with --sdk or KRYGA_ANDROID_SDK.
"""
from __future__ import annotations

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
APK_PATH = REPO_ROOT / "android" / "app" / "build" / "outputs" / "apk" / "debug" / "app-debug.apk"
APK_RELEASE_PATH = REPO_ROOT / "android" / "app" / "build" / "outputs" / "apk" / "release" / "app-release.apk"
PACKAGE = "com.kryga"
ACTIVITY = "com.kryga/.KrygaActivity"
DEFAULT_EMU = "emulator-5554"
DEFAULT_AVD = "Pixel_4_API34"
DEFAULT_NDK = "26.3.11579264"


def sdk_root() -> Path:
    override = os.environ.get("KRYGA_ANDROID_SDK")
    if override:
        return Path(override)
    local = os.environ.get("LOCALAPPDATA")
    if local:
        return Path(local) / "Android" / "Sdk"
    sys.exit("KRYGA_ANDROID_SDK / LOCALAPPDATA not set")


def adb_path() -> str:
    return str(sdk_root() / "platform-tools" / "adb.exe")


def emulator_path() -> str:
    return str(sdk_root() / "emulator" / "emulator.exe")


def ndk_stack_path(version: str = DEFAULT_NDK) -> str:
    return str(sdk_root() / "ndk" / version / "ndk-stack.cmd")


def run(cmd: list[str], *, check: bool = False, capture: bool = False,
        env: dict | None = None, timeout: float | None = None) -> subprocess.CompletedProcess:
    try:
        if capture:
            return subprocess.run(cmd, capture_output=True, text=True, check=check, env=env, timeout=timeout)
        return subprocess.run(cmd, check=check, env=env, timeout=timeout)
    except subprocess.TimeoutExpired as e:
        print(f"[timeout after {timeout}s] {' '.join(cmd[:4])} ...", file=sys.stderr)
        # Return a synthetic non-zero result so callers can continue.
        return subprocess.CompletedProcess(cmd, 124, stdout=(e.stdout or ""), stderr=(e.stderr or ""))


def adb(*args: str, device: str = DEFAULT_EMU, capture: bool = False,
        timeout: float | None = 30) -> subprocess.CompletedProcess:
    cmd = [adb_path()]
    if device:
        cmd += ["-s", device]
    cmd += list(args)
    return run(cmd, capture=capture, timeout=timeout)


# ---------------------------------------------------------------------------

def cmd_devices(args: argparse.Namespace) -> int:
    return run([adb_path(), "devices", "-l"]).returncode


def cmd_emu_start(args: argparse.Namespace) -> int:
    emu_args = [emulator_path(), "-avd", args.avd, "-no-snapshot-save", "-no-boot-anim"]
    if args.gpu:
        emu_args += ["-gpu", args.gpu]
    if args.wipe:
        emu_args.append("-wipe-data")
    print(f"Launching: {' '.join(shlex.quote(a) for a in emu_args)}")
    # Detach so we don't hold the shell.
    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    subprocess.Popen(emu_args, close_fds=True, creationflags=creationflags,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("Emulator launched in background.")
    if args.wait:
        return cmd_emu_wait(args)
    return 0


def cmd_emu_kill(args: argparse.Namespace) -> int:
    result = adb("emu", "kill", device=args.device, capture=True)
    print(result.stdout or result.stderr or "")
    return 0


def cmd_emu_wait(args: argparse.Namespace) -> int:
    deadline = time.time() + args.timeout
    last_state = ""
    while time.time() < deadline:
        r = adb("shell", "getprop", "sys.boot_completed", device=args.device, capture=True)
        state = (r.stdout or "").strip()
        if state != last_state:
            print(f"[{int(time.time()):d}] boot_completed={state!r}")
            last_state = state
        if state == "1":
            print("Emulator booted.")
            return 0
        time.sleep(3)
    print(f"Timed out after {args.timeout}s waiting for emulator boot.")
    return 1


def gradle_cmd(task: str) -> list[str]:
    gradlew = REPO_ROOT / "android" / ("gradlew.bat" if sys.platform == "win32" else "gradlew")
    return [str(gradlew), task]


def cmd_build(args: argparse.Namespace) -> int:
    task = "assembleRelease" if args.release else "assembleDebug"
    env = os.environ.copy()
    # Ensure ANDROID_HOME / JAVA_HOME are set if user didn't.
    env.setdefault("ANDROID_HOME", str(sdk_root()))
    jbr = Path("C:/Program Files/Android/Android Studio/jbr")
    if "JAVA_HOME" not in env and jbr.exists():
        env["JAVA_HOME"] = str(jbr)
        env["PATH"] = f"{jbr / 'bin'}{os.pathsep}{env.get('PATH', '')}"
    cmd = gradle_cmd(task)
    print(f"Running: {' '.join(shlex.quote(a) for a in cmd)} (cwd={REPO_ROOT / 'android'})")
    proc = subprocess.run(cmd, cwd=str(REPO_ROOT / "android"), env=env)
    return proc.returncode


def cmd_install(args: argparse.Namespace) -> int:
    apk = args.apk or (APK_RELEASE_PATH if args.release else APK_PATH)
    if not Path(apk).exists():
        print(f"APK not found: {apk}")
        return 1
    return run([adb_path(), "-s", args.device, "install", "-r", str(apk)]).returncode


def cmd_uninstall(args: argparse.Namespace) -> int:
    return run([adb_path(), "-s", args.device, "uninstall", PACKAGE]).returncode


def cmd_launch(args: argparse.Namespace) -> int:
    return adb("shell", "am", "start", "-n", ACTIVITY, device=args.device).returncode


def cmd_stop(args: argparse.Namespace) -> int:
    return adb("shell", "am", "force-stop", PACKAGE, device=args.device).returncode


def cmd_clear_logs(args: argparse.Namespace) -> int:
    # `logcat -c` often fails on vendor builds because the main buffer is
    # restricted. Ignore non-zero exit but surface stderr.
    r = adb("logcat", "-c", device=args.device, capture=True)
    if r.returncode != 0 and r.stderr:
        print(r.stderr, file=sys.stderr)
    return 0


def cmd_logs(args: argparse.Namespace) -> int:
    tag_args = []
    if args.tags:
        for t in args.tags:
            tag_args += [f"{t}:V"]
        tag_args += ["*:S"]
    else:
        tag_args = ["kryga:V", "SDL:V", "AndroidRuntime:E", "libc:F", "DEBUG:F", "*:S"]
    cmd = [adb_path(), "-s", args.device, "logcat", "-d"] + tag_args
    r = run(cmd, capture=True)
    out = r.stdout or ""
    if args.grep:
        out = "\n".join(line for line in out.splitlines() if args.grep in line)
    if args.tail:
        out = "\n".join(out.splitlines()[-args.tail:])
    print(out)
    return r.returncode


def cmd_crash(args: argparse.Namespace) -> int:
    cmd = [adb_path(), "-s", args.device, "logcat", "-d", "-b", "crash"]
    r = run(cmd, capture=True)
    out = (r.stdout or "")
    if args.tail:
        out = "\n".join(out.splitlines()[-args.tail:])
    print(out)
    return r.returncode


def cmd_symbolicate(args: argparse.Namespace) -> int:
    sym_dir = REPO_ROOT / "android" / "app" / "build" / "intermediates" / "cxx" / "Debug"
    # Pick the first variant hash directory under Debug.
    variants = [p for p in sym_dir.iterdir() if p.is_dir()] if sym_dir.exists() else []
    if not variants:
        print(f"No symbol dirs under {sym_dir}")
        return 1
    obj_dir = variants[0] / "obj" / args.abi
    if not obj_dir.exists():
        print(f"No obj dir for abi={args.abi} at {obj_dir}")
        return 1
    crash = subprocess.run(
        [adb_path(), "-s", args.device, "logcat", "-d", "-b", "crash"],
        capture_output=True, text=True, check=False,
    ).stdout
    tmp_crash = Path(os.environ.get("TEMP", "/tmp")) / "kryga_crash.log"
    tmp_crash.write_text(crash, encoding="utf-8")
    nds = ndk_stack_path(args.ndk_version)
    if not Path(nds).exists():
        print(f"ndk-stack not found: {nds}")
        return 1
    return run([nds, "-sym", str(obj_dir), "-dump", str(tmp_crash)]).returncode


def cmd_wait_for_exit(args: argparse.Namespace) -> int:
    """Poll until the app process is gone (either exited cleanly or crashed)."""
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        r = adb("shell", "pidof", PACKAGE, device=args.device, capture=True)
        pid = (r.stdout or "").strip()
        if not pid:
            print("App is not running.")
            return 0
        time.sleep(1)
    print(f"App still running after {args.timeout}s.")
    return 1


def _ensure_emu_inline(device: str, avd: str = DEFAULT_AVD, gpu: str | None = None,
                       ping_timeout: float = 5.0, boot_timeout: int = 300) -> bool:
    if _emu_healthy(device, timeout=ping_timeout):
        return True
    print("[ensure-emu] unhealthy — hard-resetting...")
    _hard_kill_qemu()
    _restart_adb()
    _launch_emu_background(avd, gpu=gpu)
    return _wait_for_boot(device, timeout=boot_timeout)


def cmd_cycle(args: argparse.Namespace) -> int:
    """build → ensure-emu → install → launch → wait → dump logs."""
    if not args.skip_build:
        rc = cmd_build(args)
        if rc != 0:
            return rc
    if not _ensure_emu_inline(args.device, avd=getattr(args, "avd", DEFAULT_AVD)):
        return 1
    rc = cmd_install(args)
    if rc != 0:
        return rc
    cmd_clear_logs(args)
    rc = cmd_launch(args)
    if rc != 0:
        return rc
    print(f"Waiting {args.wait}s for app to run / crash...")
    time.sleep(args.wait)
    return cmd_logs(args)


def cmd_adb_passthrough(args: argparse.Namespace) -> int:
    """Run any adb subcommand (for ad-hoc diagnostics)."""
    cmd = [adb_path(), "-s", args.device] + list(args.args)
    return run(cmd, timeout=args.timeout).returncode


def cmd_adb_no_device(args: argparse.Namespace) -> int:
    """Run adb without -s (e.g. kill-server, start-server, devices -l)."""
    cmd = [adb_path()] + list(args.args)
    return run(cmd, timeout=args.timeout).returncode


def cmd_restart_adb(args: argparse.Namespace) -> int:
    run([adb_path(), "kill-server"], timeout=15)
    time.sleep(1)
    run([adb_path(), "start-server"], timeout=15)
    return run([adb_path(), "devices"], timeout=10).returncode


def _emu_healthy(device: str, timeout: float = 5.0) -> bool:
    """Ping the emulator with a short timeout. True if adb shell responds."""
    r = run([adb_path(), "-s", device, "shell", "true"], capture=True, timeout=timeout)
    return r.returncode == 0


def cmd_health(args: argparse.Namespace) -> int:
    ok = _emu_healthy(args.device, timeout=args.timeout)
    print("healthy" if ok else "unhealthy")
    return 0 if ok else 1


def _hard_kill_qemu() -> int:
    import re
    r = run(["tasklist", "/FI", "IMAGENAME eq qemu-system-x86_64.exe", "/FO", "CSV"],
            capture=True, timeout=15)
    pids = re.findall(r'"qemu-system-x86_64\.exe","(\d+)"', r.stdout or "")
    rc = 0
    for pid in pids:
        r = run(["powershell", "-Command", f"Stop-Process -Id {pid} -Force"], timeout=15)
        if r.returncode != 0:
            rc = r.returncode
    return rc


def _restart_adb():
    run([adb_path(), "kill-server"], timeout=15)
    time.sleep(1)
    run([adb_path(), "start-server"], timeout=15)


def _wait_for_boot(device: str, timeout: int) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        r = run([adb_path(), "-s", device, "shell", "getprop", "sys.boot_completed"],
                capture=True, timeout=5)
        if (r.stdout or "").strip() == "1":
            return True
        time.sleep(3)
    return False


def _launch_emu_background(avd: str, gpu: str | None = None):
    args = [emulator_path(), "-avd", avd, "-no-snapshot-save", "-no-boot-anim"]
    if gpu:
        args += ["-gpu", gpu]
    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    subprocess.Popen(args, close_fds=True, creationflags=creationflags,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def _sdkmanager_cmd() -> str:
    return str(sdk_root() / "cmdline-tools" / "latest" / "bin" / "sdkmanager.bat")


def _avdmanager_cmd() -> str:
    return str(sdk_root() / "cmdline-tools" / "latest" / "bin" / "avdmanager.bat")


def _java_env() -> dict:
    env = os.environ.copy()
    jbr = Path("C:/Program Files/Android/Android Studio/jbr")
    if "JAVA_HOME" not in env and jbr.exists():
        env["JAVA_HOME"] = str(jbr)
        env["PATH"] = f"{jbr / 'bin'}{os.pathsep}{env.get('PATH', '')}"
    env.setdefault("ANDROID_HOME", str(sdk_root()))
    return env


def cmd_sdk_install(args: argparse.Namespace) -> int:
    """Install an SDK package (e.g. system-images;android-34;google_apis;x86_64)."""
    cmd = [_sdkmanager_cmd(), args.package]
    if args.channel is not None:
        cmd.insert(1, f"--channel={args.channel}")
    # Accept licenses automatically — otherwise sdkmanager interactively prompts.
    proc = subprocess.Popen(cmd, env=_java_env(), stdin=subprocess.PIPE, text=True)
    try:
        # Feed a stream of "y" answers for the license prompts.
        proc.communicate(input="y\n" * 50, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        print(f"[timeout after {args.timeout}s] sdkmanager install", file=sys.stderr)
        return 124
    return proc.returncode


def cmd_sdk_list(args: argparse.Namespace) -> int:
    flag = "--list_installed" if args.installed else "--list"
    cmd = [_sdkmanager_cmd(), flag]
    r = run(cmd, capture=True, env=_java_env(), timeout=120)
    out = r.stdout or ""
    if args.grep:
        out = "\n".join(l for l in out.splitlines() if args.grep.lower() in l.lower())
    print(out)
    return r.returncode


def cmd_avd_create(args: argparse.Namespace) -> int:
    """Create an AVD from an installed system image."""
    cmd = [_avdmanager_cmd(), "create", "avd",
           "-n", args.name,
           "-k", args.package,
           "-d", args.device,
           "--force"]
    # avdmanager asks "Do you wish to create a custom hardware profile? [no]" — feed "no".
    proc = subprocess.Popen(cmd, env=_java_env(), stdin=subprocess.PIPE, text=True)
    try:
        proc.communicate(input="no\n", timeout=args.timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        return 124
    return proc.returncode


def cmd_avd_list(args: argparse.Namespace) -> int:
    return run([emulator_path(), "-list-avds"], env=_java_env(), timeout=15).returncode


def cmd_avd_delete(args: argparse.Namespace) -> int:
    return run([_avdmanager_cmd(), "delete", "avd", "-n", args.name],
               env=_java_env(), timeout=30).returncode


def cmd_ensure_emu(args: argparse.Namespace) -> int:
    """Make sure a healthy emulator is up; recover if zombie.

    Recovery steps:
      - If adb shell doesn't respond within `ping_timeout`:
          hard-kill QEMU, restart adb, relaunch emulator, wait for boot.
    """
    if _emu_healthy(args.device, timeout=args.ping_timeout):
        print("emulator healthy")
        return 0
    print("emulator unhealthy — recovering...")
    _hard_kill_qemu()
    _restart_adb()
    _launch_emu_background(args.avd, gpu=args.gpu)
    print("waiting for boot...")
    if _wait_for_boot(args.device, timeout=args.boot_timeout):
        print("emulator booted.")
        return 0
    print("emulator failed to boot.")
    return 1


def cmd_qemu_pids(args: argparse.Namespace) -> int:
    """List running Android-emulator QEMU processes (Windows)."""
    cmd = ["tasklist", "/FI", "IMAGENAME eq qemu-system-x86_64.exe"]
    return run(cmd, timeout=15).returncode


def cmd_emu_kill_hard(args: argparse.Namespace) -> int:
    """Force-kill the QEMU emulator process on Windows (when adb emu kill fails)."""
    import re
    r = run(["tasklist", "/FI", "IMAGENAME eq qemu-system-x86_64.exe", "/FO", "CSV"],
            capture=True, timeout=15)
    pids = []
    for line in (r.stdout or "").splitlines():
        m = re.match(r'"qemu-system-x86_64\.exe","(\d+)"', line)
        if m:
            pids.append(m.group(1))
    if not pids:
        print("No running emulator QEMU processes.")
        return 0
    rc = 0
    for pid in pids:
        print(f"Killing PID {pid}")
        r = run(["powershell", "-Command", f"Stop-Process -Id {pid} -Force"], timeout=15)
        if r.returncode != 0:
            rc = r.returncode
    return rc


def cmd_logcat_stream(args: argparse.Namespace) -> int:
    """Stream adb logcat to stdout for args.duration seconds, then stop.

    Useful when the emulator dies during launch and `logcat -d` afterwards
    returns nothing. Pipe to a file via shell redirection, or use --out.
    """
    tag_args = []
    if args.tags:
        for t in args.tags:
            tag_args += [f"{t}:V"]
        tag_args += ["*:S"]
    cmd = [adb_path(), "-s", args.device, "logcat", "-T", "0"] + tag_args
    out_f = None
    if args.out:
        out_f = open(args.out, "w", encoding="utf-8", errors="replace")
    try:
        proc = subprocess.Popen(cmd, stdout=(out_f or sys.stdout),
                                stderr=subprocess.STDOUT, text=True)
        try:
            proc.wait(timeout=args.duration)
        except subprocess.TimeoutExpired:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        return 0
    finally:
        if out_f:
            out_f.close()


def cmd_capture(args: argparse.Namespace) -> int:
    """ensure-emu → install → start streaming logs → launch → wait → stop → print filtered.

    Captures logs to a file so we don't lose them even if the emulator crashes.
    Auto-recovers a zombie emulator before running.
    """
    out = args.out or os.path.join(os.environ.get("TEMP", "/tmp"), "kryga_capture.log")
    if not _ensure_emu_inline(args.device, avd=getattr(args, "avd", DEFAULT_AVD)):
        return 1
    if not args.skip_install:
        rc = cmd_install(args)
        if rc != 0:
            return rc
    # Start logcat stream in background.
    tag_args = []
    if args.tags:
        for t in args.tags:
            tag_args += [f"{t}:V"]
        tag_args += ["*:S"]
    logcat_cmd = [adb_path(), "-s", args.device, "logcat", "-T", "0"] + tag_args
    print(f"Streaming logcat to {out} for {args.duration}s")
    with open(out, "w", encoding="utf-8", errors="replace") as f:
        proc = subprocess.Popen(logcat_cmd, stdout=f, stderr=subprocess.STDOUT, text=True)
        try:
            # Give logcat a moment to connect.
            time.sleep(1)
            # Launch the app.
            rc = cmd_launch(args)
            if rc != 0:
                print(f"launch failed rc={rc}")
            # Wait.
            try:
                proc.wait(timeout=args.duration)
            except subprocess.TimeoutExpired:
                pass
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
    # Print tail with grep.
    try:
        with open(out, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"(no output file at {out})")
        return 0
    if args.grep:
        lines = [l for l in lines if args.grep in l]
    if args.tail:
        lines = lines[-args.tail:]
    print("".join(lines))
    print(f"--- full log: {out} ({len(lines)} filtered lines) ---")
    return 0


# ---------------------------------------------------------------------------

def main() -> int:
    p = argparse.ArgumentParser(description="Kryga Android dev helper")
    p.add_argument("--device", default=DEFAULT_EMU, help="adb -s target (default: emulator-5554)")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("devices", help="list adb devices")
    sp.set_defaults(func=cmd_devices)

    sp = sub.add_parser("emu-start", help="start the AVD in background")
    sp.add_argument("--avd", default=DEFAULT_AVD)
    sp.add_argument("--gpu", default=None, help="e.g. swiftshader_indirect, host")
    sp.add_argument("--wipe", action="store_true")
    sp.add_argument("--wait", action="store_true", help="also wait for boot")
    sp.add_argument("--timeout", type=int, default=300)
    sp.set_defaults(func=cmd_emu_start)

    sp = sub.add_parser("emu-kill", help="kill the AVD via adb emu kill")
    sp.set_defaults(func=cmd_emu_kill)

    sp = sub.add_parser("emu-wait", help="poll for sys.boot_completed=1")
    sp.add_argument("--timeout", type=int, default=300)
    sp.set_defaults(func=cmd_emu_wait)

    sp = sub.add_parser("build", help="./gradlew assembleDebug")
    sp.add_argument("--release", action="store_true")
    sp.set_defaults(func=cmd_build)

    sp = sub.add_parser("install", help="install debug APK on device")
    sp.add_argument("--release", action="store_true")
    sp.add_argument("--apk", help="explicit APK path (overrides --release)")
    sp.set_defaults(func=cmd_install)

    sp = sub.add_parser("uninstall", help="uninstall the app")
    sp.set_defaults(func=cmd_uninstall)

    sp = sub.add_parser("launch", help="start main activity")
    sp.set_defaults(func=cmd_launch)

    sp = sub.add_parser("stop", help="force-stop the app")
    sp.set_defaults(func=cmd_stop)

    sp = sub.add_parser("clear-logs", help="adb logcat -c")
    sp.set_defaults(func=cmd_clear_logs)

    sp = sub.add_parser("logs", help="dump logcat (-d) with a sane filter")
    sp.add_argument("--tags", nargs="*", help="override log tag set")
    sp.add_argument("--grep", help="filter to lines containing this substring")
    sp.add_argument("--tail", type=int, default=50)
    sp.set_defaults(func=cmd_logs)

    sp = sub.add_parser("crash", help="dump adb logcat -b crash")
    sp.add_argument("--tail", type=int, default=40)
    sp.set_defaults(func=cmd_crash)

    sp = sub.add_parser("symbolicate", help="run ndk-stack against the latest crash buffer")
    sp.add_argument("--abi", default="x86_64")
    sp.add_argument("--ndk-version", default=DEFAULT_NDK)
    sp.set_defaults(func=cmd_symbolicate)

    sp = sub.add_parser("wait-exit", help="poll until the app process is gone")
    sp.add_argument("--timeout", type=int, default=60)
    sp.set_defaults(func=cmd_wait_for_exit)

    sp = sub.add_parser("cycle", help="build → install → launch → wait → dump logs")
    sp.add_argument("--release", action="store_true")
    sp.add_argument("--skip-build", action="store_true")
    sp.add_argument("--wait", type=int, default=15, help="seconds between launch and log dump")
    sp.add_argument("--tags", nargs="*")
    sp.add_argument("--grep")
    sp.add_argument("--tail", type=int, default=60)
    sp.add_argument("--apk", help="explicit APK path (overrides --release)")
    sp.set_defaults(func=cmd_cycle)

    sp = sub.add_parser("capture", help="install → stream logs → launch → wait → tail logs")
    sp.add_argument("--release", action="store_true")
    sp.add_argument("--skip-install", action="store_true")
    sp.add_argument("--apk")
    sp.add_argument("--duration", type=int, default=15)
    sp.add_argument("--tags", nargs="*", default=["kryga", "SDL", "AndroidRuntime", "libc", "DEBUG"])
    sp.add_argument("--grep")
    sp.add_argument("--tail", type=int, default=120)
    sp.add_argument("--out")
    sp.set_defaults(func=cmd_capture)

    sp = sub.add_parser("logcat-stream", help="stream adb logcat to stdout or --out for N seconds")
    sp.add_argument("--tags", nargs="*", default=["kryga", "SDL", "AndroidRuntime", "libc", "DEBUG"])
    sp.add_argument("--duration", type=int, default=15)
    sp.add_argument("--out")
    sp.set_defaults(func=cmd_logcat_stream)

    sp = sub.add_parser("adb", help="run arbitrary adb subcommand on the selected device")
    sp.add_argument("args", nargs=argparse.REMAINDER)
    sp.add_argument("--timeout", type=int, default=30)
    sp.set_defaults(func=cmd_adb_passthrough)

    sp = sub.add_parser("adb-raw", help="run arbitrary adb subcommand without -s (kill/start-server, devices, etc.)")
    sp.add_argument("args", nargs=argparse.REMAINDER)
    sp.add_argument("--timeout", type=int, default=30)
    sp.set_defaults(func=cmd_adb_no_device)

    sp = sub.add_parser("restart-adb", help="kill-server + start-server + devices")
    sp.set_defaults(func=cmd_restart_adb)

    sp = sub.add_parser("qemu-pids", help="list running emulator QEMU processes")
    sp.set_defaults(func=cmd_qemu_pids)

    sp = sub.add_parser("emu-kill-hard", help="force-kill QEMU emulator process (Windows)")
    sp.set_defaults(func=cmd_emu_kill_hard)

    sp = sub.add_parser("health", help="quick ping — is the emulator responsive?")
    sp.add_argument("--timeout", type=float, default=5.0)
    sp.set_defaults(func=cmd_health)

    sp = sub.add_parser("ensure-emu",
                        help="ensure a healthy emulator; auto-recover (hard-kill + relaunch) if zombie")
    sp.add_argument("--avd", default=DEFAULT_AVD)
    sp.add_argument("--gpu", default=None)
    sp.add_argument("--ping-timeout", type=float, default=5.0)
    sp.add_argument("--boot-timeout", type=int, default=300)
    sp.set_defaults(func=cmd_ensure_emu)

    sp = sub.add_parser("sdk-install", help="install an SDK package via sdkmanager")
    sp.add_argument("package", help='e.g. "system-images;android-34;google_apis;x86_64"')
    sp.add_argument("--channel", default=None)
    sp.add_argument("--timeout", type=int, default=1200)
    sp.set_defaults(func=cmd_sdk_install)

    sp = sub.add_parser("sdk-list", help="list SDK packages (default: installed)")
    sp.add_argument("--installed", action="store_true", default=True)
    sp.add_argument("--all", dest="installed", action="store_false")
    sp.add_argument("--grep")
    sp.set_defaults(func=cmd_sdk_list)

    sp = sub.add_parser("avd-create", help="create an AVD from an installed system image")
    sp.add_argument("--name", required=True, help="AVD name, e.g. Pixel_4_API34")
    sp.add_argument("--package", required=True, help='e.g. "system-images;android-34;google_apis;x86_64"')
    sp.add_argument("--device", default="pixel_4")
    sp.add_argument("--timeout", type=int, default=120)
    sp.set_defaults(func=cmd_avd_create)

    sp = sub.add_parser("avd-list", help="list AVDs known to the emulator")
    sp.set_defaults(func=cmd_avd_list)

    sp = sub.add_parser("avd-delete", help="delete an AVD")
    sp.add_argument("--name", required=True)
    sp.set_defaults(func=cmd_avd_delete)

    args = p.parse_args()
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())

"""
Launch Filament gltf_viewer and capture a screenshot after it renders.
Usage: python tools/visual_compare/capture.py <scene.glb> <output.png> [--width 1024] [--height 1024]

Requires: pip install pillow
"""
import subprocess
import sys
import time
import os
import argparse

def find_filament_viewer():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    viewer = os.path.join(root, "build", "filament", "bin", "gltf_viewer.exe")
    if os.path.exists(viewer):
        return viewer, os.path.join(root, "build", "filament", "bin")
    return None, None

def main():
    parser = argparse.ArgumentParser(description="Capture Filament render")
    parser.add_argument("scene", help="Path to .glb file")
    parser.add_argument("output", help="Output .png path")
    parser.add_argument("--width", type=int, default=1024)
    parser.add_argument("--height", type=int, default=1024)
    parser.add_argument("--delay", type=float, default=3.0, help="Seconds to wait before capture")
    args = parser.parse_args()

    viewer, bin_dir = find_filament_viewer()
    if not viewer:
        print("Filament not found. Run: tools/fetch_filament.sh")
        sys.exit(1)

    scene = os.path.abspath(args.scene)
    if not os.path.exists(scene):
        print(f"Scene not found: {scene}")
        sys.exit(1)

    try:
        from PIL import ImageGrab
        import ctypes
    except ImportError:
        print("pip install pillow")
        sys.exit(1)

    print(f"Launching Filament: {os.path.basename(scene)}")
    proc = subprocess.Popen(
        [viewer, "--api", "vulkan", scene],
        cwd=bin_dir,
    )

    print(f"Waiting {args.delay}s for render...")
    time.sleep(args.delay)

    if proc.poll() is not None:
        print("Filament exited unexpectedly")
        sys.exit(1)

    # Find the Filament window and capture it
    import ctypes.wintypes

    user32 = ctypes.windll.user32

    def find_window_by_pid(pid):
        result = []
        def callback(hwnd, _):
            tid, wpid = ctypes.wintypes.DWORD(), ctypes.wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
            if wpid.value == pid and user32.IsWindowVisible(hwnd):
                result.append(hwnd)
            return True
        WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_int, ctypes.POINTER(ctypes.c_int))
        user32.EnumWindows(WNDENUMPROC(callback), 0)
        return result[0] if result else None

    hwnd = find_window_by_pid(proc.pid)
    if not hwnd:
        print("Could not find Filament window")
        proc.terminate()
        sys.exit(1)

    # Get window rect
    rect = ctypes.wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))

    # Capture
    bbox = (rect.left, rect.top, rect.right, rect.bottom)
    img = ImageGrab.grab(bbox)
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    img.save(args.output)
    print(f"Saved: {args.output} ({img.size[0]}x{img.size[1]})")

    proc.terminate()
    proc.wait()

if __name__ == "__main__":
    main()

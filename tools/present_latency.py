#!/usr/bin/env python3
"""Measure kryga's render->display latency with PresentMon (Windows).

PresentMon captures present/display events from outside the app via ETW, so it
needs no engine instrumentation and is the ground-truth way to compare present
modes (mailbox vs fifo) and frames-in-flight settings.

Usage:
    # capture the running editor for 10s and summarize
    python tools/present_latency.py --seconds 10

    # target a different process / a saved CSV
    python tools/present_latency.py --process kryga_game.exe --seconds 15
    python tools/present_latency.py --csv capture.csv      # parse only, no capture

Requirements:
    - PresentMon.exe on PATH, or pass --presentmon <path>, or set PRESENTMON env.
      Get it from https://github.com/GameTechDev/PresentMon/releases
    - Run from an ELEVATED shell — ETW capture needs admin.

What it reports (per displayed frame):
    - Frame time      : ms between presents (cadence / FPS)
    - Display latency  : ms from present submit to scanout (render->display)
Numbers are avg / median / p95 / p99 so you can compare modes meaningfully.
"""

import argparse
import csv
import os
import shutil
import statistics
import subprocess
import sys
import tempfile


def find_presentmon(explicit):
    for cand in (explicit, os.environ.get("PRESENTMON"), "PresentMon.exe", "presentmon"):
        if not cand:
            continue
        path = shutil.which(cand) if os.path.basename(cand) == cand else cand
        if path and os.path.exists(path):
            return path
    return None


def capture(presentmon, process, seconds, out_csv):
    # Flags accepted by PresentMon 1.6+ and 2.x. --stop_existing_session avoids
    # "already tracing" errors; --terminate_after_timed exits when the timer
    # elapses. NOTE: 1.x's -no_top was removed in 2.x — passing it aborts the
    # capture. ETW needs elevation; --restart_as_admin self-elevates via UAC
    # (interactive) when the shell isn't already admin.
    cmd = [
        presentmon,
        "--process_name", process,
        "--output_file", out_csv,
        "--timed", str(seconds),
        "--terminate_after_timed",
        "--stop_existing_session",
        "--no_console_stats",
    ]
    print(f"[capture] {' '.join(cmd)}", flush=True)
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print(f"[capture] PresentMon exited {r.returncode} "
              f"(elevated shell? process '{process}' running?)", file=sys.stderr)
    return os.path.exists(out_csv)


def pick(headers, *needles):
    """First header containing all needle substrings (case-insensitive)."""
    low = [(h, h.lower()) for h in headers]
    for h, hl in low:
        if all(n in hl for n in needles):
            return h
    return None


def collect(csv_path):
    """Parse a PresentMon CSV into a stats dict (no printing). Returns None on
    empty CSV. Keys: frame/disp/gpu/api -> stats dict (or None), modes -> {name: count}."""
    with open(csv_path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return None

    headers = rows[0].keys()
    frame_col = pick(headers, "msbetweenpresents") or pick(headers, "frametime")
    disp_col = (pick(headers, "msuntildisplayed")
                or pick(headers, "until", "displayed")
                or pick(headers, "displaylatency"))
    mode_col = pick(headers, "presentmode")
    gpu_col = pick(headers, "gpulatency") or pick(headers, "msgpuactive") \
        or pick(headers, "msuntilrendercomplete")
    api_col = pick(headers, "msinpresentapi") or pick(headers, "inpresentapi")

    def stats(col):
        if not col:
            return None
        vals = []
        for r in rows:
            try:
                v = float(r[col])
            except (TypeError, ValueError):
                continue
            if v > 0:
                vals.append(v)
        if not vals:
            return None
        vals.sort()
        p = lambda q: vals[min(len(vals) - 1, int(q * len(vals)))]
        return {
            "n": len(vals), "avg": statistics.fmean(vals), "med": statistics.median(vals),
            "p95": p(0.95), "p99": p(0.99), "max": vals[-1],
        }

    modes = {}
    if mode_col:
        for r in rows:
            modes[r[mode_col]] = modes.get(r[mode_col], 0) + 1

    return {
        "rows": len(rows),
        "frame": stats(frame_col), "disp": stats(disp_col),
        "gpu": stats(gpu_col), "api": stats(api_col), "modes": modes,
    }


def summarize(csv_path):
    with open(csv_path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        print("no rows captured — was the app presenting frames?", file=sys.stderr)
        return 1

    headers = rows[0].keys()
    # Column names drift across PresentMon versions; match by substring.
    frame_col = pick(headers, "msbetweenpresents") or pick(headers, "frametime")
    disp_col = (pick(headers, "msuntildisplayed")
                or pick(headers, "until", "displayed")
                or pick(headers, "displaylatency"))
    # Diagnostic columns: which present path the OS used, and the GPU/CPU split.
    # PresentMode == "Composed: Flip" means windowed DWM composition (extra
    # flip-queue latency that vkWaitForPresent can't see); "Hardware: Independent
    # Flip" means DWM handed off to the scanout hardware (low latency). This is
    # THE column that separates our render-ahead queue from compositor latency.
    mode_col = pick(headers, "presentmode")
    gpu_col = pick(headers, "gpulatency") or pick(headers, "msgpuactive") \
        or pick(headers, "msuntilrendercomplete")
    api_col = pick(headers, "msinpresentapi") or pick(headers, "inpresentapi")

    def stats(col):
        if not col:
            return None
        vals = []
        for r in rows:
            try:
                v = float(r[col])
            except (TypeError, ValueError):
                continue
            if v > 0:
                vals.append(v)
        if not vals:
            return None
        vals.sort()
        p = lambda q: vals[min(len(vals) - 1, int(q * len(vals)))]
        return {
            "n": len(vals), "avg": statistics.fmean(vals), "med": statistics.median(vals),
            "p95": p(0.95), "p99": p(0.99), "max": vals[-1],
        }

    def line(label, s):
        if not s:
            print(f"  {label:<16}: column not found in CSV")
            return
        print(f"  {label:<16}: avg {s['avg']:6.2f}  med {s['med']:6.2f}  "
              f"p95 {s['p95']:6.2f}  p99 {s['p99']:6.2f}  max {s['max']:6.2f}  (n={s['n']})")

    fs = stats(frame_col)
    print(f"\nPresentMon summary ({len(rows)} presents)  [ms]")
    line("Frame time", fs)
    line("Display latency", stats(disp_col))
    line("GPU latency", stats(gpu_col))
    line("In present API", stats(api_col))
    if fs:
        print(f"  {'FPS (from avg)':<16}: {1000.0 / fs['avg']:.1f}")

    # PresentMode distribution — the diagnostic. If this says "Composed: Flip"
    # the frames go through DWM and the deep display latency is the compositor
    # flip queue, not anything vkQueuePresentKHR pacing alone can fix.
    if mode_col:
        counts = {}
        for r in rows:
            counts[r[mode_col]] = counts.get(r[mode_col], 0) + 1
        print(f"  {'PresentMode':<16}: ", end="")
        print(", ".join(f"{k} ({v})" for k, v in
                         sorted(counts.items(), key=lambda kv: -kv[1])))
    else:
        print(f"  {'PresentMode':<16}: column not found in CSV")
    print()
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--process", default="kryga_editor.exe", help="target process name")
    ap.add_argument("--seconds", type=int, default=10, help="capture duration")
    ap.add_argument("--presentmon", help="path to PresentMon.exe")
    ap.add_argument("--csv", help="parse an existing CSV instead of capturing")
    args = ap.parse_args()

    if args.csv:
        return summarize(args.csv)

    pm = find_presentmon(args.presentmon)
    if not pm:
        print("PresentMon.exe not found. Install it (GameTechDev/PresentMon releases), "
              "add to PATH, set PRESENTMON, or pass --presentmon.", file=sys.stderr)
        return 2

    out = os.path.join(tempfile.gettempdir(), "kryga_presentmon.csv")
    if os.path.exists(out):
        os.remove(out)
    if not capture(pm, args.process, args.seconds, out):
        return 3
    return summarize(out)


if __name__ == "__main__":
    sys.exit(main())

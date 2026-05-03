#!/usr/bin/env python3
"""
Side-by-side before/after PNG combiner for visual regression review.

Usage:
    python tools/diff_image.py BEFORE_PNG AFTER_PNG [OUT_PNG]

Default OUT_PNG: build/project_Debug/tmp/diff_combined.png

Designed for the workflow:
    1. cp resources/test_references/<name>.png /tmp/before.png
    2. UPDATE_REFERENCES=1 tools/run.sh visual_regression_tests.exe --gtest_filter=...
    3. python tools/diff_image.py /tmp/before.png resources/test_references/<name>.png

Produces a stacked image (before on left, after on right) with labels and a
small gap, so the eye can A/B-compare between iterations.
"""

import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


def label_font(size=16):
    for candidate in ("arial.ttf", "DejaVuSans.ttf", "Helvetica.ttf"):
        try:
            return ImageFont.truetype(candidate, size)
        except (OSError, IOError):
            continue
    return ImageFont.load_default()


def combine(before_path: Path, after_path: Path, out_path: Path) -> None:
    before = Image.open(before_path).convert("RGBA")
    after = Image.open(after_path).convert("RGBA")

    h = max(before.height, after.height)
    if before.height != h:
        before = before.resize((int(before.width * h / before.height), h))
    if after.height != h:
        after = after.resize((int(after.width * h / after.height), h))

    gap = 4
    total_w = before.width + after.width + gap
    combined = Image.new("RGB", (total_w, h), (32, 32, 32))
    combined.paste(before, (0, 0))
    combined.paste(after, (before.width + gap, 0))

    draw = ImageDraw.Draw(combined)
    font = label_font(16)
    pad = 4
    label_h = 22

    draw.rectangle([0, 0, 80, label_h], fill=(0, 0, 0))
    draw.text((pad, 2), "BEFORE", fill=(255, 255, 255), font=font)
    after_x = before.width + gap
    draw.rectangle([after_x, 0, after_x + 70, label_h], fill=(0, 0, 0))
    draw.text((after_x + pad, 2), "AFTER", fill=(255, 255, 255), font=font)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    combined.save(out_path)
    print(f"Saved: {out_path} ({total_w}x{h})")


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    before = Path(sys.argv[1])
    after = Path(sys.argv[2])
    if len(sys.argv) >= 4:
        out = Path(sys.argv[3])
    else:
        out = Path("build/project_Debug/tmp/diff_combined.png")

    if not before.exists():
        print(f"BEFORE not found: {before}", file=sys.stderr)
        return 1
    if not after.exists():
        print(f"AFTER not found: {after}", file=sys.stderr)
        return 1

    combine(before, after, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

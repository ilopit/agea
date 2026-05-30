"""Swapchain recreation — verify rendering data survives a rebuild.

Changing frames_in_flight or present mode routes through
vulkan_render::reconfigure_swapchain, which recreates the swapchain and
re-seeds the per-frame GPU buffer slots. Growing the in-flight count creates
fresh slots seeded from a survivor (clone_contents_from + pending-queue copy);
shrinking frees slots; a pure present-mode switch touches no per-frame buffers.

The render cache (obj->gpu_data) is the source of truth and is NOT touched by a
swapchain rebuild — so render.object.data / render.stats alone can't prove the
*per-frame SSBOs* were re-seeded correctly. The only external signal that
actually exercises the seeded buffers is the rendered image. So the core check
captures one screenshot per live slot after each rebuild (cycling through every
slot) and asserts the scene stays identical to a pre-rebuild baseline. A broken
re-seed would leave a grown slot empty/garbage, and that slot's frame would
diverge hard from the baseline.

Assumes a STATIC editor scene (fixed camera, no animation). The test measures a
two-frame noise floor first and skips if the scene isn't static, since the
diff comparison would be meaningless otherwise.
"""
import base64
from io import BytesIO

import pytest
from PIL import Image, ImageChops

from .conftest import EXPECTED_RENDER_OBJECTS
from . import assertions

# Center crop fraction — keeps the comparison over the 3D scene (cube/ground/
# grid) and away from ImGui panels / perf overlays at the edges.
CROP_FRAC = 0.5

# Mean per-channel abs pixel diff (0..255) below which two frames are "the same
# scene". Floor for when the measured dither/UI noise is tiny.
DIFF_FLOOR = 6.0
# Skip the visual check if the static-scene assumption is violated (camera or
# animation moving), i.e. two consecutive frames already differ this much.
NOISE_SKIP = DIFF_FLOOR


def _decode(shot: dict) -> Image.Image:
    data = shot["image"]
    if data.startswith("data:"):
        data = data.split(",", 1)[1]
    return Image.open(BytesIO(base64.b64decode(data))).convert("RGB")


def _center_crop(img: Image.Image, frac: float = CROP_FRAC) -> Image.Image:
    w, h = img.size
    bw, bh = int(w * frac), int(h * frac)
    x0, y0 = (w - bw) // 2, (h - bh) // 2
    return img.crop((x0, y0, x0 + bw, y0 + bh))


def _mean_abs_diff(a: Image.Image, b: Image.Image) -> float:
    assert a.size == b.size, f"image size changed: {a.size} vs {b.size}"
    hist = ImageChops.difference(a, b).histogram()  # R[256] G[256] B[256]
    total = 0
    count = 0
    for ch in range(3):
        base = ch * 256
        for v in range(256):
            n = hist[base + v]
            total += v * n
            count += n
    return total / count if count else 0.0


def _shot_crop(engine) -> Image.Image:
    return _center_crop(_decode(engine.screenshot()))


def _obj_gpu(engine, obj_id: str) -> dict:
    return engine.call("render.object.data", {"id": obj_id})["gpu_data"]


class TestSwapchainRecreation:

    def _baseline(self, engine):
        """Reference crop + measured two-frame noise floor."""
        ref = _shot_crop(engine)
        engine.wait_frame()
        ref2 = _shot_crop(engine)
        return ref, _mean_abs_diff(ref, ref2)

    def _assert_scene_survives(self, engine, ref, threshold, live_slots, label):
        """Capture one frame per live slot (+1) and assert each matches ref.

        Cycling live_slots+1 frames guarantees every seeded slot is presented at
        least once, so a slot left empty by a bad re-seed can't hide."""
        worst = 0.0
        for _ in range(live_slots + 1):
            d = _mean_abs_diff(ref, _shot_crop(engine))
            worst = max(worst, d)
            engine.wait_frame()
        assert worst < threshold, (
            f"scene diverged after {label}: max pixel diff {worst:.2f} "
            f"exceeds threshold {threshold:.2f} — a grown slot likely rendered "
            f"stale/empty data (per-frame SSBO not re-seeded)"
        )

    def test_data_survives_grow_shrink_and_present_switch(self, engine):
        cfg0 = engine.call("render.config.get")
        orig_fif = cfg0["frames_in_flight"]
        orig_present = cfg0["present_mode_name"]

        # Render-cache + stats baseline (must be untouched by any rebuild).
        stats0 = engine.call("render.stats")
        gpu0 = {o: _obj_gpu(engine, o) for o in EXPECTED_RENDER_OBJECTS}

        ref, noise = self._baseline(engine)
        if noise > NOISE_SKIP:
            pytest.skip(
                f"scene not static (two-frame noise {noise:.2f} > {NOISE_SKIP}) "
                f"— screenshot diff check needs a still scene"
            )
        threshold = max(noise * 4.0, DIFF_FLOOR)

        # (present, frames_in_flight). Covers grow, shrink, present-only switch
        # (new == old count → no per-frame buffer work), and every present mode.
        steps = [
            ("fifo", 2),       # settle to a known start (likely a rebuild)
            ("fifo", 4),       # GROW 2->4: seeds new slots from a survivor
            ("fifo", 2),       # SHRINK 4->2: frees slots
            ("mailbox", 3),    # grow + present change (mailbox forces >=3)
            ("fifo", 3),       # PRESENT-ONLY switch at count 3 (no buffer work)
            ("immediate", 2),  # shrink + present change
            (orig_present, orig_fif),  # restore
        ]

        for present, fif in steps:
            engine.call(
                "render.config.set",
                {"present_mode": present, "frames_in_flight": fif},
            )
            engine.wait_frame(3)  # let apply_pending run + swapchain settle

            applied = engine.call("render.config.get")
            live = applied["frames_in_flight"]
            label = f"{present}/{fif} (applied {applied['present_mode_name']}/{live})"

            # 1. The rendered scene survives the rebuild on EVERY live slot.
            self._assert_scene_survives(engine, ref, threshold, live, label)

            # 2. Render cache is intact (counts + per-object GPU data unchanged).
            stats = engine.call("render.stats")
            assert stats["object_count"] == stats0["object_count"], (
                f"object_count changed after {label}"
            )
            assert stats["directional_light_count"] == stats0["directional_light_count"]
            assert stats["universal_light_count"] == stats0["universal_light_count"]

            for o in EXPECTED_RENDER_OBJECTS:
                assert _obj_gpu(engine, o) == gpu0[o], (
                    f"gpu_data for '{o}' changed after {label}"
                )
                ro = engine.call("render.object.data", {"id": o})
                assertions.assert_not_pink_bug(ro, o)

        # Config restored to where we started.
        final = engine.call("render.config.get")
        assert final["frames_in_flight"] == orig_fif
        assert final["present_mode_name"] == orig_present

    def test_repeated_grow_shrink_is_stable(self, engine):
        """Hammer the grow/shrink path several times — a leak or a stale clone
        source would drift the scene or counts over iterations."""
        cfg0 = engine.call("render.config.get")
        orig_fif = cfg0["frames_in_flight"]
        orig_present = cfg0["present_mode_name"]
        stats0 = engine.call("render.stats")

        ref, noise = self._baseline(engine)
        if noise > NOISE_SKIP:
            pytest.skip(f"scene not static (noise {noise:.2f})")
        threshold = max(noise * 4.0, DIFF_FLOOR)

        for i in range(4):
            for fif in (4, 2):
                engine.call(
                    "render.config.set",
                    {"present_mode": "fifo", "frames_in_flight": fif},
                )
                engine.wait_frame(3)
                applied = engine.call("render.config.get")
                self._assert_scene_survives(
                    engine, ref, threshold, applied["frames_in_flight"],
                    f"iter {i} fif={fif}",
                )
                assert engine.call("render.stats")["object_count"] == stats0["object_count"]

        # restore
        engine.call(
            "render.config.set",
            {"present_mode": orig_present, "frames_in_flight": orig_fif},
        )
        engine.wait_frame(3)

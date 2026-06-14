"""Audio subsystem e2e — clip assets, emitter component, and the import RPC.

Audio is a *model-only* subsystem: the audio_system is a pure consumer with no
render-layer projection, and sound output is not observable over RPC. So unlike
the mesh/material tests there is deliberately no render-layer assertion here —
these tests pin the model + serialization + wiring surface:

  - audio_clip assets load from the package and expose probed metadata
    (sample rate / channels / duration / format) — exercises audio_clip
    reflection and the uint32 `audio_format` enum round-trip off disk.
  - audio_emitter_component is registered and its properties (including the
    audio_clip pointer reference) round-trip through the property system.
  - editor.audio.import decodes a real file and writes an audio_clip asset.

Serialization round-trip (save + reload) is intentionally out of scope: only
authored content is persisted by model.level.save, so runtime-added components
(any type) do not survive reload — that round-trip belongs in a model-layer test.

Requires the editor built WITH the uncommitted audio changes
(`cmake --preset host` then rebuild). The stock build has no `audio_clip`
architype and no audio_emitter_component, so these tests fail at model.list /
listTypes against an un-rebuilt engine — that's a missing build, not a bug.
"""
import math
import struct
import wave
import pytest

from base import audio_emitter_component
from root import audio_clip
from .rpc_client import PROJECT_ROOT
from .property_helpers import create_test_object, add_component, cleanup_object

# Pre-imported into base.apkg (resources/packages/base.apkg/class/audio_clips/beep.*).
CLIP_ID = "beep"
EMITTER_TYPE = "audio_emitter_component"

# Where editor.audio.import writes (absolute → independent of the editor CWD,
# which is build/project_<cfg>/bin, not the repo root).
BASE_PKG = PROJECT_ROOT / "resources" / "packages" / "base.apkg"
AUDIO_CLIPS_DIR = BASE_PKG / "class" / "audio_clips"


def _list_audio_clip_ids(engine):
    result = engine.call("model.list", {"source": "all", "kind": "audio_clip"})
    return [c["id"] for c in result.get("items", [])]


def _write_wav(path, hz=8000, secs=0.1, freq=440.0):
    """Synthesize a tiny valid 16-bit mono PCM WAV miniaudio can decode."""
    n = int(hz * secs)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(hz)
        w.writeframes(
            b"".join(
                struct.pack("<h", int(0.3 * 32767 * math.sin(2 * math.pi * freq * i / hz)))
                for i in range(n)
            )
        )


# ---------------------------------------------------------------------------
# audio_clip asset
# ---------------------------------------------------------------------------

class TestAudioClipAsset:
    """The pre-imported `beep` clip loads from base.apkg and exposes metadata.

    This is the read side of the import pipeline: if the architype, cache, or
    package-manifest wiring is wrong the clip never loads; if the uint32 enum
    round-trip is wrong the format field comes back garbage.
    """

    def test_clip_listed(self, engine):
        """model.list(kind=audio_clip) resolves the architype and returns beep."""
        ids = _list_audio_clip_ids(engine)
        assert CLIP_ID in ids, f"'{CLIP_ID}' not in loaded audio clips: {ids}"

    def test_clip_metadata(self, engine):
        """Probed metadata is sane — proves the descriptor round-tripped off disk."""
        clip = audio_clip(engine, CLIP_ID)
        sample_rate = clip.get_sample_rate()
        channels = clip.get_channels()
        duration = clip.get_duration_seconds()
        fmt = clip.get_format()

        assert isinstance(sample_rate, int) and sample_rate > 0, f"sample_rate={sample_rate}"
        assert channels in (1, 2), f"channels={channels}"
        assert isinstance(duration, (int, float)) and duration > 0.0, f"duration={duration}"
        # audio_format: unknown=0, wav=1, mp3=2, flac=3, ogg_vorbis=4. A real clip
        # must not be unknown, and the uint32 enum must survive serialization.
        assert isinstance(fmt, int) and 1 <= fmt <= 4, f"format={fmt}"


# ---------------------------------------------------------------------------
# audio_emitter_component
# ---------------------------------------------------------------------------

class TestAudioEmitterComponent:
    """The emitter is a registered component whose properties round-trip and
    persist. No playback is asserted (not observable over RPC, and CI may have no
    audio device) — this is purely the model/serialization surface."""

    def test_type_registered(self, engine):
        """audio_emitter_component shows up in the addable component types."""
        types = engine.call("model.component.listTypes")
        type_ids = {t["type_id"] for t in types}
        assert EMITTER_TYPE in type_ids, f"{EMITTER_TYPE} not registered: {sorted(type_ids)}"

    def test_add_and_roundtrip(self, engine):
        """Add an emitter and round-trip each property through the property system."""
        obj_id = create_test_object(engine, "audio_emitter_rt_obj")
        try:
            comp_id = add_component(engine, obj_id, EMITTER_TYPE, "emitter_rt")
            emitter = audio_emitter_component(engine, comp_id)

            # Bind the asset pointer by id (same path as mesh/material handles).
            emitter.set_clip(CLIP_ID)
            assert emitter.get_clip() == CLIP_ID

            emitter.set_volume(0.5)
            emitter.set_loop(True)
            emitter.set_autoplay(False)
            emitter.set_spatial(False)
            emitter.set_min_distance(2.5)
            emitter.set_max_distance(40.0)

            assert abs(emitter.get_volume() - 0.5) < 1e-4
            assert emitter.get_loop() is True
            assert emitter.get_autoplay() is False
            assert emitter.get_spatial() is False
            assert abs(emitter.get_min_distance() - 2.5) < 1e-4
            assert abs(emitter.get_max_distance() - 40.0) < 1e-4
        finally:
            cleanup_object(engine, obj_id)

    def test_clip_reference_binds(self, engine):
        """Binding the clip pointer by id resolves and reads back through the
        property system — the editor-inspector path for assigning a clip.

        NOTE: serialization round-trip (save + reload) is deliberately NOT tested
        here. model.component.add creates a *runtime* component, and model.level.save
        only persists authored content — runtime-added components (any type, not just
        audio) do not survive reload. The emitter/clip save/load round-trip belongs
        in a model-layer GTest against an authored object, not an e2e RPC test."""
        obj_id = create_test_object(engine, "audio_clip_bind_obj")
        try:
            comp_id = add_component(engine, obj_id, EMITTER_TYPE, "emitter_bind")
            emitter = audio_emitter_component(engine, comp_id)

            assert emitter.get_clip() in (None, ""), "clip should start unset"
            emitter.set_clip(CLIP_ID)
            assert emitter.get_clip() == CLIP_ID, (
                "clip reference did not bind/echo — check the asset-pointer resolve "
                "list in property_rpc.cpp"
            )
        finally:
            cleanup_object(engine, obj_id)


# ---------------------------------------------------------------------------
# editor.audio.import RPC
# ---------------------------------------------------------------------------

class TestAudioImportRpc:
    """The import RPC validates params and decodes + writes a real asset."""

    def test_missing_params_errors(self, engine):
        """No input/id/package → handler reports an error (deterministic, no files)."""
        with pytest.raises(RuntimeError):
            engine.call("editor.audio.import", {})

    def test_import_wav(self, engine, tmp_path):
        """Synthesize a WAV, import it into base.apkg, and confirm the asset files
        are written. Uses a unique id and removes the two outputs afterwards so the
        package tree is left exactly as found."""
        import_id = "e2e_audio_import_test"
        src = tmp_path / "tone.wav"
        _write_wav(src)

        out_obj = AUDIO_CLIPS_DIR / f"{import_id}.aobj"
        out_aaud = AUDIO_CLIPS_DIR / f"{import_id}.aaud"
        try:
            result = engine.call("editor.audio.import", {
                "input": str(src),
                "id": import_id,
                "package": str(BASE_PKG),
            })
            assert result.get("ok") is True, f"import not ok: {result}"
            assert result.get("id") == import_id

            assert out_obj.is_file(), f"descriptor not written: {out_obj}"
            assert out_aaud.is_file(), f"payload not written: {out_aaud}"
            assert out_aaud.stat().st_size > 0, "payload is empty"
        finally:
            for f in (out_obj, out_aaud):
                if f.exists():
                    f.unlink()


# ---------------------------------------------------------------------------
# Playback — audible verification with --slow
# ---------------------------------------------------------------------------

class TestAudioPlayback:
    """Drive the whole pipeline live: model emit → audio_bridge → audio_system.

    There is no RPC to observe a voice, so this can't assert sound itself — but it
    exercises the full play path (a crash or regression in the bridge would fail
    here), and with --slow you VERIFY BY EAR. The authored hero_cube_beeper on
    simple_test (loop + autoplay) makes the pattern unmistakable:

        beep ......  (play)
        silence ...  (exit — proves end_play stops the voice)
        beep ......  (re-enter — proves autoplay re-triggers)

    Run audibly (4s per phase, -s so prints show):

        python -m pytest tests/e2e/test_audio.py -k Playback --slow 4 -s

    Default (no --slow) it still runs fast and just covers play/exit/re-enter.
    """

    def _set_mode(self, engine, mode):
        engine.call_queued("engine.setMode", {"mode": mode})
        engine.wait_frame(8)
        assert engine.call("engine.getMode")["mode"] == mode

    def test_emitter_authored_on_hero_cube(self, engine):
        """Sanity: the looping autoplay emitter is present before we play it."""
        props = engine.call("model.object.property.get", {"id": "hero_cube"})
        owner_ids = {o["id"] for o in props.get("owners", [])}
        assert "hero_cube_beeper" in owner_ids, (
            f"authored emitter missing from hero_cube: {sorted(owner_ids)}"
        )

    def test_autoplay_play_stop_replay(self, engine, slow):
        """play → exit → re-enter, holding each phase so it's audible under --slow."""
        try:
            self._set_mode(engine, "play")
            slow("PLAYING: looping beep should be audible")

            self._set_mode(engine, "edit")
            slow("STOPPED: should be silent (end_play stopped the voice)")

            self._set_mode(engine, "play")
            slow("PLAYING AGAIN: autoplay re-triggered on re-enter")
        finally:
            # Always return to edit so the next test starts clean.
            engine.call_queued("engine.setMode", {"mode": "edit"})
            engine.wait_frame(3)

    def test_spatial_distance_attenuation(self, engine, slow):
        """3D attenuation gradient driven by moving the PARENT game object, so the
        whole cube (its mesh + the emitter component) moves together and you can
        confirm it by eye as well as by ear. Audible with --slow:
        LOUD -> MEDIUM -> QUIET -> SILENT as the cube recedes.

        Moving hero_cube_root recomputes the subtree, so both the renderable mesh
        and the non-renderable emitter follow — the sound stays on the cube. The
        listener is the play-mode camera (~[0,8,15], conftest TEST_CAMERA); the cube
        moves back along z. With min_distance=9 / max_distance=40 the emitter's
        distance to the listener gives:
            near  [0,0,10]  ~9u   -> full volume
            mid   [0,0,2]   ~15u  -> ~80%
            far   [0,0,-8]  ~24u  -> ~50%
            vfar  [0,0,-25] ~41u  -> past max_distance, inaudible
        Play starts ONCE; the cube then jumps between distances while the voice
        keeps looping (logged as 'voice ... moved to (...)'), so the tone never
        restarts — only its volume changes as the cube moves away.
        """
        beeper = "hero_cube_beeper"
        root = "hero_cube_root"   # the parent — moving it carries mesh + emitter
        PHASES = [
            ([0, 0, 10], "NEAR (~9u): LOUD"),
            ([0, 0, 2], "MIDDLE (~15u): ~80% volume"),
            ([0, 0, -8], "FAR (~24u): ~50% volume"),
            ([0, 0, -25], "VERY FAR (~41u): SILENT (past max_distance)"),
        ]
        START = PHASES[0][0]

        def set_prop(owner, name, value):
            engine.call("model.object.property.set",
                        {"owner_id": owner, "name": name, "value": value})

        # 3D config must be set BEFORE play: play() bakes spatial + min/max + the
        # initial source position into the voice at begin_play.
        set_prop(beeper, "spatial", True)
        set_prop(beeper, "min_distance", 9.0)
        set_prop(beeper, "max_distance", 40.0)
        set_prop(root, "position", START)
        engine.wait_frame()

        try:
            # Start the voice ONCE, then move the parent — mesh + emitter follow.
            self._set_mode(engine, "play")
            for pos, label in PHASES:
                set_prop(root, "position", pos)
                engine.wait_frame(3)   # let the subtree update + on_tick stream it
                slow(label)
        finally:
            engine.call_queued("engine.setMode", {"mode": "edit"})
            engine.wait_frame(3)

    def test_spatial_listener_distance(self, engine, slow):
        """Same attenuation, but moving the CAMERA (listener) instead of the source.

        The cube stays at the orbit center; the play-mode camera zooms out via
        player_0.orbit_radius, so the listener-to-source distance grows and the
        volume drops. Audible with --slow, and visible as the camera pulls back
        from the cube. The camera orbits [0,0,0] at distance == orbit_radius, and
        the source sits there too, so distance-to-listener == orbit_radius.
        With min_distance=4 / max_distance=40:
            radius 3   -> < min, full volume
            radius 12  -> ~78%
            radius 24  -> ~44%
            radius 45  -> past max_distance, inaudible
        player_0 only exists in play mode, so orbit_radius is set after entering.
        """
        beeper = "hero_cube_beeper"
        root = "hero_cube_root"
        PHASES = [
            (3.0, "NEAR (radius 3): LOUD"),
            (12.0, "MIDDLE (radius 12): ~78% volume"),
            (24.0, "FAR (radius 24): ~44% volume"),
            (45.0, "VERY FAR (radius 45): SILENT (past max_distance)"),
        ]

        def set_prop(owner, name, value):
            engine.call("model.object.property.set",
                        {"owner_id": owner, "name": name, "value": value})

        # Source spatial and parked AT the orbit center so distance == orbit_radius.
        set_prop(beeper, "spatial", True)
        set_prop(beeper, "min_distance", 4.0)
        set_prop(beeper, "max_distance", 40.0)
        set_prop(root, "position", [0, 0, 0])
        engine.wait_frame()

        try:
            self._set_mode(engine, "play")   # spawns player_0 (the orbit camera)
            for radius, label in PHASES:
                set_prop("player_0", "orbit_radius", radius)
                engine.wait_frame(6)         # let the orbit settle to the new radius
                cam = engine.call("render.camera.data", {})["position"]
                dist = math.sqrt(sum(c * c for c in cam))
                assert abs(dist - radius) < 1.5, (
                    f"camera distance {dist:.1f} != orbit_radius {radius}"
                )
                slow(label)
        finally:
            engine.call_queued("engine.setMode", {"mode": "edit"})
            engine.wait_frame(3)

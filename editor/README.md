# Kryga VS Code editor integration

Phase 1 (polling frame transport). The engine runs headless when launched
with `--editor-ipc <name>`, publishes every rendered frame into a shared-
memory region, and the VS Code webview polls that region at 60 Hz.

Phase 2 replaces the poll loop with a signaled wait and adds the input
return path.

## Engine CLI (Phase 0)

```
engine_app --editor-ipc <name>        # Headless, offscreen render, IPC placeholder
engine_app --dump-first-frame <path>  # Headless, render warm-up burst, save PNG, exit
engine_app --headless-width <W>       # Headless render target width  (default 1024)
engine_app --headless-height <H>      # Headless render target height (default 1024)
engine_app --wait-for-debugger        # Print PID and block until debugger attaches
```

## Shared-memory name scheme (reserved for Phase 1)

The `<name>` passed to `--editor-ipc` is a bare identifier (letters, digits,
`_`, `-`). Each OS derives concrete primitive names from it:

| OS       | Shared memory region         | Frame-ready event / semaphore | Input-available event / semaphore |
| -------- | ---------------------------- | ----------------------------- | --------------------------------- |
| Linux    | `/kryga_editor_<name>`       | `/kryga_editor_<name>_fr`     | `/kryga_editor_<name>_ia`         |
| macOS    | `/kryga_editor_<name>`       | `/kryga_editor_<name>_fr`     | `/kryga_editor_<name>_ia`         |
| Windows  | `Local\kryga_editor_<name>`  | `Local\kryga_editor_<name>_fr` | `Local\kryga_editor_<name>_ia`   |

The leading `/` on POSIX and the `Local\` prefix on Windows are added by the
platform layer, not by callers. POSIX names are limited to 30 characters total
(SHM_NAME_MAX on macOS), so `<name>` must fit within ~14 ASCII characters for
cross-platform safety.

Launcher convention (TS side launches engine):

```
name = "session_" + process.pid.toString(16);
engine_app --editor-ipc ${name}
```

The engine unlinks stale shared-memory regions matching its own `<name>` at
startup, before creating a new one. The launcher (or the user) is responsible
for picking a name that does not collide with other live sessions.

## VS Code extension

Located in `editor/vscode/`.

```
editor/vscode/
  src/               TypeScript extension source
  media/             Webview HTML + viewport.js (WebGL blitter)
  native/            N-API addon (cmake-js build)
    src/addon.cpp    JS bindings for the shm reader
    CMakeLists.txt   Reuses libs/editor_ipc/ for the shm wrapper
```

Build and run:

```
cd editor/vscode/native
npm install                     # Builds the native .node via cmake-js
cd ..
npm install && npm run compile  # Compiles the TS extension
```

Open the `editor/vscode/` folder in VS Code and press F5 to launch an
Extension Development Host. In the host, run `> Kryga: Open Viewport`
(Ctrl+Shift+P) and enter the same `<name>` you passed to the engine's
`--editor-ipc` flag. The viewport panel attaches to the shared-memory
region and displays published frames.

### Frame pipeline (Phase 1)

```
engine draw_headless
   └─ render graph writes main_pass.color[frame_idx]  (VkImage)
        └─ frame_publisher.publish()
             ├─ vkCmdCopyImageToBuffer  →  host-visible staging buffer
             ├─ memcpy (BGRA→RGBA if needed)  →  shm slot pixel blob
             └─ atomic store latest_ready_slot, fetch_add frame_counter

addon.readHeader / getSlotBuffer  (polled every 16ms from ext host)
   └─ Buffer.from(slice)   — copy out of mmap while publisher is locked out
        └─ postMessage     — structured-clone into webview

webview.onmessage('frame')
   └─ gl.texSubImage2D  →  draw fullscreen triangle
```

Frames are therefore copied three times between GPU and canvas (staging →
shm → post-message → WebGL texture). Phase 1 accepts this; Phase 2 adds the
signaled wait but does not reduce copies. The copy count is noted so it's
easy to find when profiling later.

## Property inspector (Phase 4)

`Kryga: Open Property Inspector` opens a side panel bound to the same IPC
channel as the viewport. Phase 4 wires exactly one component — the
edit-camera position — through a drag-scrub Vector3 input. This proves the
full round trip:

```
UI drag         postMessage → extension
                postMessageIn("setProperty path=camera.position.x value=1.5")
engine tick     drain_messages_in → handle_ipc_message
                -> game_editor::set_camera_position(...)
                -> push_message_out("propertyChanged path=... value=...")
extension poll  drainMessagesOut → webview postMessage
                -> inspector script updates its selection snapshot
```

Wire format is intentionally un-JSON: each record is a single line of
`verb key=value ...` ASCII tokens, so the engine doesn't need a JSON
parser. The full message rings (64 slots × 512 bytes each, one ring per
direction) live in the same shared-memory region as the frame slots.
Ring overflow drops the outgoing message (drag-scrub is naturally
idempotent so dropped `setProperty`s just mean the next preview tick
wins).

### Scope of Phase 4

- `requestSelection` returns a hardcoded `selection entity=play_camera`.
  Entity selection by clicking the viewport is Phase 5 work.
- Only `camera.position.{x,y,z}` paths are handled by the engine. Adding
  paths means adding case branches in `vulkan_engine::handle_ipc_message`
  until Phase 5 replaces the dispatcher with a reflection walk.

## Reflection bridge (Phase 5 — scaffold)

Phase 5 wires the engine's existing reflection system (`KRG_ar_*`
macros, argen.py, `reflection_type_registry`) into the IPC schema
stream. On every consumer attach the engine walks every registered
type and pushes one record of the form

```
schema type=Transform f=position:17:0 f=rotation:18:12 f=scale:17:28
```

to the editor, where each `f=` triplet is `name:type_id:offset`. The
inspector caches these schemas keyed by type name.

What's in scope for this phase:

- `frame_publisher::is_consumer_attached()` — lets the engine re-emit
  schemas on every reconnect (late-attaching editor still gets them).
- `vulkan_engine::send_schemas()` — registry walk. Records longer than
  `MSG_SLOT_BYTES` (512) are truncated by `push_message_out`; chunking
  is documented as a follow-up but not needed for current type widths.
- Inspector schema cache — populated on `schema` records.

What's intentionally *not* in this phase:

- **Generic `setProperty` dispatch**. The engine still routes property
  edits through the hardcoded Phase 4 switch in `handle_ipc_message`.
  Replacing it requires a type-safe writer API on
  `reflection::property` — something like
  `property::set_value(obj, std::string_view value_text)` — which
  touches every type's argen-generated code and is out of scope for the
  IPC layer. When that exists, the dispatcher becomes a two-line
  registry lookup.
- **Generic inspector rendering**. The schema cache is populated but the
  UI still renders the hardcoded camera-position Vector3. Wiring
  type_id → widget (float, vec3, quat, bool, enum) is the natural next
  step once the engine writer API lands.
- **Entity selection by click** (raycast in viewport → entity id → push
  `selection`). The engine's picking path exists
  (`object_id_under_coordinate`) and can be reused; not wired here.

Phase 5 is thus a *capability* scaffold: the schema channel proves the
extension can render any registered component once the writer API is in
place. The original plan described Phase 5 as "optional, defer until
painful" — treating it as scaffolding keeps that deferral honest.

## Diagnostics (Phase 3)

### `editor_ipc_shmdump`

Attaches to a running region without disturbing the publisher or the
editor:

```
tools/run.sh editor_ipc_shmdump <name> <max_width> <max_height> [out.png]
```

Prints every header atomic (magic, version, dims, frame_counter,
latest_ready_slot, publisher_alive, consumer_attached, input ring
head/tail) and optionally saves the most recent ready slot to a PNG.
Useful when the editor shows a black viewport: if shmdump shows frames
arriving, the problem is in the extension / webview; if shmdump sees
frame_counter frozen, it's upstream.

### Robustness behaviors

- **Engine crashes mid-session.** The extension's 1 Hz watchdog reads
  `publisher_alive == 0` and triggers a reconnect loop (retries
  `open()` every 500 ms). No VS Code restart needed. On the next
  engine start, `frame_publisher::init()` calls
  `shared_memory::unlink_stale()` to clear the POSIX region before
  creating a fresh one with the same name.
- **Extension starts before engine.** `open()` returns -1 until the
  engine creates the region; the viewport shows "disconnected" and
  retries every 500 ms.
- **Extension disconnects mid-frame.** `reading_slot` is cleared in a
  `finally` block so the publisher is never stuck thinking a consumer
  is still reading a slot.
- **Engine resizes the viewport.** `frame_publisher::resize(w, h)`
  updates `current_width/height` and bumps `generation`. The consumer
  reads `generation` twice around each frame copy (seqlock style); a
  mismatch discards the in-flight frame. Resize must stay within the
  `max_width`/`max_height` provisioned at `init()` — a true max-dim
  change is a full tear-down and reconnect.
- **Ring overflow.** `postInput` returns false when the input ring is
  full (256 slots). The extension currently drops the event rather
  than blocking the JS thread. Noted here so a future "batch input"
  sender knows it must handle rejection.

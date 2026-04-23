// Mirror of libs/editor_ipc/public/include/editor_ipc/frame_protocol.h.
// Hand-ported rather than generated to keep the addon dependency-free. If
// the C++ layout changes, bump SHM_VERSION on both sides; the addon refuses
// to attach on mismatch.

export const SHM_MAGIC = 0x4b524741; // 'KRGA'
export const SHM_VERSION = 2;
export const NUM_SLOTS = 3;
export const SLOT_NONE = 0xffffffff;
export const CACHE_LINE = 64;
export const INPUT_RING_CAPACITY = 256;
export const INPUT_EVENT_BYTES = 24;

export enum PixelFormat {
  Rgba8 = 0,
  Bgra8 = 1,
}

function alignUp(value: number, alignment: number): number {
  return (value + alignment - 1) & ~(alignment - 1);
}

// sizeof(frame_header) in bytes. Must match the C++ layout; see comment in
// frame_protocol.h. Field-by-field offsets with alignment:
//
//   8 × uint32        = 32 (magic..slot_bytes)
//   3 × uint64        = 24 (slot_offsets[3]) — offset 32..56
//   atomic<u32> gen   =  4 — offset 56
//   atomic<u64> ctr   =  8 — needs 8-byte align, pad to 64 → 64..72
//   6 × atomic<u32>   = 24 (latest..consumer_attached) — 72..96
//   uint64 ring_off   =  8 — 96..104
//   uint32 ring_cap   =  4 — 104..108
//   uint32 _reserved0 =  4 — 108..112
//   atomic<u32> head  =  4 — 112..116
//   atomic<u32> tail  =  4 — 116..120
//                     ---
//                      120 bytes (8-byte aligned, no trailing padding).
export const FRAME_HEADER_BYTES = 120;

export function computeRegionBytes(maxWidth: number, maxHeight: number): number {
  const slotBytes = maxWidth * 4 * maxHeight;
  const ringBytes = INPUT_RING_CAPACITY * INPUT_EVENT_BYTES;
  let cursor = alignUp(FRAME_HEADER_BYTES, CACHE_LINE);
  for (let i = 0; i < NUM_SLOTS; ++i) {
    cursor += alignUp(slotBytes, CACHE_LINE);
  }
  cursor += alignUp(ringBytes, CACHE_LINE);
  return cursor;
}

export interface FrameHeader {
  magic: number;
  version: number;
  maxWidth: number;
  maxHeight: number;
  pixelFormat: number;
  strideBytes: number;
  numSlots: number;
  slotBytes: number;
  generation: number;
  frameCounter: bigint;
  latestReadySlot: number;
  readingSlot: number;
  currentWidth: number;
  currentHeight: number;
  publisherAlive: number;
  consumerAttached: number;
  inputRingOffset: bigint;
  inputRingCapacity: number;
  inputRingHead: number;
  inputRingTail: number;
}

export enum InputEventType {
  MouseMove = 0,
  MouseButton = 1,
  MouseWheel = 2,
  Key = 3,
}

export interface InputEvent {
  type: InputEventType;
  timestampMs: number;
  a: number;
  b: number;
  c: number;
  d: number;
}

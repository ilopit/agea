// Mirror of libs/editor_ipc/public/include/editor_ipc/frame_protocol.h.
// Hand-ported rather than generated to keep the addon dependency-free. If
// the C++ layout changes, bump SHM_VERSION on both sides; the addon refuses
// to attach on mismatch.

export const SHM_MAGIC = 0x4b524741; // 'KRGA'
export const SHM_VERSION = 3;
export const NUM_SLOTS = 3;
export const SLOT_NONE = 0xffffffff;
export const CACHE_LINE = 64;
export const INPUT_RING_CAPACITY = 256;
export const INPUT_EVENT_BYTES = 24;
export const MSG_RING_CAPACITY = 64;
export const MSG_SLOT_BYTES = 512;

export enum PixelFormat {
  Rgba8 = 0,
  Bgra8 = 1,
}

function alignUp(value: number, alignment: number): number {
  return (value + alignment - 1) & ~(alignment - 1);
}

// sizeof(frame_header) in bytes, Phase 4 layout. Extends Phase 2's header
// with two control-message rings (in + out):
//
//   120 bytes (Phase 2 header)
//   +  8 msg_in_offset  u64
//   +  8 msg_out_offset u64
//   +  4 msg_ring_capacity
//   +  4 msg_slot_bytes
//   +  4 msg_in_head
//   +  4 msg_in_tail
//   +  4 msg_out_head
//   +  4 msg_out_tail
//   = 160 bytes (8-byte aligned, no trailing padding)
export const FRAME_HEADER_BYTES = 160;

export function computeRegionBytes(maxWidth: number, maxHeight: number): number {
  const slotBytes = maxWidth * 4 * maxHeight;
  const inputRingBytes = INPUT_RING_CAPACITY * INPUT_EVENT_BYTES;
  const msgRingBytes = MSG_RING_CAPACITY * MSG_SLOT_BYTES;
  let cursor = alignUp(FRAME_HEADER_BYTES, CACHE_LINE);
  for (let i = 0; i < NUM_SLOTS; ++i) {
    cursor += alignUp(slotBytes, CACHE_LINE);
  }
  cursor += alignUp(inputRingBytes, CACHE_LINE);
  cursor += alignUp(msgRingBytes, CACHE_LINE); // msg_in
  cursor += alignUp(msgRingBytes, CACHE_LINE); // msg_out
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

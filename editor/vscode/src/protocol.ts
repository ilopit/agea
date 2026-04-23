// Mirror of libs/editor_ipc/public/include/editor_ipc/frame_protocol.h.
// Hand-ported rather than generated to keep the addon dependency-free. If
// the C++ layout changes, bump SHM_VERSION on both sides; the addon refuses
// to attach on mismatch.

export const SHM_MAGIC = 0x4b524741; // 'KRGA'
export const SHM_VERSION = 1;
export const NUM_SLOTS = 3;
export const SLOT_NONE = 0xffffffff;
export const CACHE_LINE = 64;

export enum PixelFormat {
  Rgba8 = 0,
  Bgra8 = 1,
}

function alignUp(value: number, alignment: number): number {
  return (value + alignment - 1) & ~(alignment - 1);
}

// Size of the frame_header struct in bytes. Must match the C++ layout; the
// simplest way to stay honest is to list every field:
//
//   9 × uint32       = 36
//   3 × uint64       = 24  (slot_offsets)
//   1 × uint32       =  4  (generation, atomic<uint32_t>)
//   1 × uint64       =  8  (frame_counter, atomic<uint64_t>)
//   6 × uint32       = 24  (latest_ready, reading, cur_w, cur_h, pub_alive, consumer_attached)
//   1 × uint64       =  8  (input_ring_offset)
//   2 × uint32       =  8  (input_ring_size, _reserved0)
//                    ---
//                     112 bytes
//
// Plus trailing alignment to alignof(frame_header). std::atomic<T> has the
// same alignof as T on every sane platform; the struct alignment ends up
// being 8 (the largest uint64_t member). 112 is already 8-byte aligned.
export const FRAME_HEADER_BYTES = 112;

export function computeRegionBytes(maxWidth: number, maxHeight: number): number {
  const slotBytes = maxWidth * 4 * maxHeight;
  let cursor = alignUp(FRAME_HEADER_BYTES, CACHE_LINE);
  for (let i = 0; i < NUM_SLOTS; ++i) {
    cursor += alignUp(slotBytes, CACHE_LINE);
  }
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
}

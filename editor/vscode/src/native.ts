// Native addon loader. Compiled by cmake-js into ../native/build/Release/
// during `npm install` in the native/ directory. Wrapped here in a typed
// facade so the rest of the extension can stay addon-agnostic.

import * as path from "path";
import { FrameHeader } from "./protocol";

export interface KrygaNative {
  open(name: string, sizeBytes: number): number;
  close(handle: number): void;
  readHeader(handle: number): FrameHeader;
  getSlotBuffer(handle: number, slotIndex: number): Buffer;
  claimSlot(handle: number, slotIndex: number): void;
  releaseSlot(handle: number): void;
}

let cached: KrygaNative | null = null;

export function loadNative(): KrygaNative {
  if (cached) return cached;

  const candidates = [
    path.resolve(__dirname, "../native/build/Release/kryga_native.node"),
    path.resolve(__dirname, "../native/build/Debug/kryga_native.node"),
  ];

  for (const candidate of candidates) {
    try {
      cached = require(candidate) as KrygaNative;
      return cached;
    } catch {
      // Try the next candidate.
    }
  }

  throw new Error(
    `kryga_native addon not found. Build it first:\n` +
      `    cd editor/vscode/native && npm install\n\n` +
      `Searched:\n  ${candidates.join("\n  ")}`,
  );
}

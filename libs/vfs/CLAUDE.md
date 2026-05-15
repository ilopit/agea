# VFS — Virtual File System

Layered virtual filesystem with multiple backends and priority-based resolution.

## Resource IDs (`rid.h`)
Format: `"mount://relative/path"` with constexpr parsing. Hashable, supports path composition via `/` operator.

## Mounting
- `virtual_file_system` manages mount entries mapping mount points to backends
- Backends: `physical_backend` (real filesystem), `memory_backend` (in-memory)
- **Priority sorting:** stable-sorted by mount_point (lexicographic) then priority (descending) — higher priority backends are queried first within the same mount point
- **Scoped mounts:** backends mounted at a subpath; scope prefix is stripped before backend query, enabling overlays at specific directories

## Read vs write resolution
- **Read:** iterates all backends at a mount point by priority
- **Write:** uses separate `m_write_targets` map, defaults to first writable backend

## File index (`file_index.h`)
Optional per-mount stem-based lookup. Scans directories and builds stem→path map. Respects `load_order` for prefix-based iteration priority.

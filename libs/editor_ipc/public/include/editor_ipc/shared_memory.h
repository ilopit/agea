#pragma once

// Thin cross-platform shared-memory wrapper. Used by the engine (create +
// writer) and — via a mirror copy under editor/vscode/native — by the N-API
// addon (attach + reader).
//
// The wrapper owns exactly one handle + one mapped view. It does not know
// anything about the frame protocol; see frame_protocol.h for the layout.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace kryga::editor_ipc
{

class shared_memory
{
public:
    enum class mode
    {
        create,  // Create region (unlinking any stale one with the same name).
        attach,  // Attach to an existing region.
    };

    shared_memory() = default;
    ~shared_memory();

    shared_memory(const shared_memory&) = delete;
    shared_memory& operator=(const shared_memory&) = delete;

    shared_memory(shared_memory&& other) noexcept;
    shared_memory& operator=(shared_memory&& other) noexcept;

    // Opens a region. `name` is the bare identifier; the wrapper prepends
    // the OS-specific prefix (see editor/README.md for the scheme). On
    // `create`, `size_bytes` must be nonzero and determines the region size;
    // on `attach`, `size_bytes` must match whatever the publisher created.
    //
    // Returns true on success. On failure, the object is left in a closed
    // state and `last_error()` holds a human-readable reason.
    bool
    open(std::string_view name, mode m, size_t size_bytes);

    void
    close();

    bool
    is_open() const
    {
        return m_base != nullptr;
    }

    void*
    data()
    {
        return m_base;
    }
    const void*
    data() const
    {
        return m_base;
    }

    size_t
    size() const
    {
        return m_size;
    }

    const std::string&
    last_error() const
    {
        return m_last_error;
    }

    // Static helper: best-effort removal of a stale region with the given
    // name. No-op on failure. Called implicitly by open(create); exposed so
    // the engine can sweep on startup without creating.
    static void
    unlink_stale(std::string_view name);

private:
    void* m_base = nullptr;
    size_t m_size = 0;
    // Platform handle as an opaque value; intptr_t works on both POSIX (fd)
    // and Windows (HANDLE cast to intptr_t).
    intptr_t m_handle = -1;
    std::string m_name;  // Raw user-supplied name, without platform prefix.
    std::string m_last_error;
    bool m_created = false;  // True if this process created (as opposed to attached to) the region.
};

}  // namespace kryga::editor_ipc

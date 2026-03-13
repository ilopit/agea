#pragma once

#include <utils/id.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace kryga
{

namespace render
{
class vulkan_render;
class vulkan_render_loader;
}  // namespace render

class render_command_processor;

namespace render_cmd
{

// Opaque handle to render-thread-owned resources.
// Main thread stores these; render thread resolves them to pointers.
using render_handle = uint32_t;
inline constexpr render_handle k_invalid_handle = UINT32_MAX;

struct render_exec_context
{
    render_command_processor& processor;
    render::vulkan_render& vr;
    render::vulkan_render_loader& loader;
};

struct render_command_base
{
    virtual ~render_command_base() = default;
    virtual void execute(render_exec_context& ctx) = 0;
};

// Linear arena for per-frame command allocation.
// Main thread allocates via alloc<T>().
// Render thread calls execute() then dtor.
// Main thread calls reset() after render is done.
class cmd_arena
{
    static constexpr size_t k_default_capacity = 4 * 1024 * 1024;  // 4 MB

public:
    explicit cmd_arena(size_t capacity = k_default_capacity)
        : m_buf(capacity)
    {
    }

    template <typename T, typename... Args>
    T*
    alloc(Args&&... args)
    {
        static constexpr size_t align = alignof(T);
        static constexpr size_t size = sizeof(T);

        size_t aligned = (m_offset + align - 1) & ~(align - 1);
        assert(aligned + size <= m_buf.size());

        auto* ptr = new (m_buf.data() + aligned) T(std::forward<Args>(args)...);
        m_offset = aligned + size;
        return ptr;
    }

    void
    reset()
    {
        m_offset = 0;
    }

    size_t
    used() const
    {
        return m_offset;
    }

    size_t
    capacity() const
    {
        return m_buf.size();
    }

private:
    std::vector<std::byte> m_buf;
    size_t m_offset = 0;
};

}  // namespace render_cmd
}  // namespace kryga

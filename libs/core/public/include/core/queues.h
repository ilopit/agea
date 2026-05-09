#pragma once

#include <core/caches/line_cache.h>

#include <packages/root/model/components/game_object_component.h>

#include <utils/memory_arena.h>
#include <utils/spsc_queue.h>

namespace kryga
{

namespace render_cmd
{
struct render_command_base;
}

namespace core
{

static constexpr size_t k_cache_line = 64;

class queues
{
public:
    // Model-side dirty tracking (main thread only)
    struct alignas(k_cache_line) model
    {
        line_cache<root::game_object_component*> dirty_transforms;
        line_cache<root::smart_object*> dirty_render;

        void
        drop_pending()
        {
            dirty_transforms.clear();
            dirty_render.clear();
        }
    };

    // Render command queue (main thread pushes, render thread drains)
    struct alignas(k_cache_line) render
    {
        utils::memory_arena arena;
        utils::spsc_queue<render_cmd::render_command_base*> command_queue{16384};

        template <typename T, typename... Args>
        T*
        alloc_cmd(Args&&... args)
        {
            return arena.alloc<T>(std::forward<Args>(args)...);
        }

        void
        enqueue(render_cmd::render_command_base* cmd)
        {
            command_queue.push(std::move(cmd));
        }

        // Retire the current arena so its memory survives until the render
        // thread has drained all commands allocated from it.  A fresh arena
        // becomes active for subsequent allocations.
        void
        retire_arena()
        {
            m_pending_retire.push_back(std::move(arena));
            arena = utils::memory_arena{};
        }

        // Reset the active arena and release arenas that the render thread
        // has already drained.  Arenas retired since the last reset move
        // into the draining slot — they survive one more render cycle.
        void
        reset_arena()
        {
            arena.reset();
            m_draining_arenas.clear();
            m_draining_arenas.swap(m_pending_retire);
        }

    private:
        std::vector<utils::memory_arena> m_pending_retire;
        std::vector<utils::memory_arena> m_draining_arenas;
    };

    model&
    get_model()
    {
        return m_model;
    }

    render&
    get_render()
    {
        return m_render;
    }

private:
    model m_model;
    render m_render;
};

}  // namespace core
}  // namespace kryga

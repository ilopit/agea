#pragma once

#include <core/caches/line_cache.h>

#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/assets/asset.h>
#include <packages/root/model/assets/shader_effect.h>

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
        line_cache<root::game_object_component*> dirty_render_components;
        line_cache<root::asset*> dirty_render_assets;
        line_cache<root::shader_effect*> dirty_shader_effects;

        void
        drop_pending()
        {
            dirty_transforms.clear();
            dirty_render_components.clear();
            dirty_render_assets.clear();
            dirty_shader_effects.clear();
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

        void
        reset_arena()
        {
            arena.reset();
        }
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

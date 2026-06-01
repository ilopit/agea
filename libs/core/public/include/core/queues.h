#pragma once

#include <core/caches/line_cache.h>

#include <packages/root/model/components/game_object_component.h>

namespace kryga
{
namespace core
{

static constexpr size_t k_cache_line = 64;

// Model-side dirty tracking only (main thread). The render command queue is a
// separate subsystem now — see vulkan_render/input_queue.h (render::input_queue).
class queues
{
public:
    // Model-side dirty tracking (main thread only)
    struct alignas(k_cache_line) model
    {
        line_cache<root::game_object_component*> dirty_transforms;
        line_cache<root::smart_object*> dirty_render;
        line_cache<root::smart_object*> destroy_render;
        std::vector<std::shared_ptr<root::smart_object>> deferred_release;

        void
        drop_pending()
        {
            dirty_transforms.clear();
            dirty_render.clear();
            destroy_render.clear();
            deferred_release.clear();
        }
    };

    model&
    get_model()
    {
        return m_model;
    }

private:
    model m_model;
};

}  // namespace core
}  // namespace kryga

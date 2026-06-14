#pragma once

#include <core/caches/line_cache.h>
#include <core/audio_message.h>

#include <packages/root/model/components/game_object_component.h>

#include <vector>

namespace kryga
{
namespace core
{

static constexpr size_t k_cache_line = 64;

// The model's per-frame output to the render side (main thread only), owned by
// model_system and reached via getr_model().output: the dirty/destroy lists the
// render pipeline drains each frame, plus deferred_release (objects held alive
// until end-of-frame). The render command queue is a separate render-side
// subsystem — see vulkan_render/input_queue.h (render::input_queue).
struct alignas(k_cache_line) model_output
{
    line_cache<root::game_object_component*> dirty_transforms;
    line_cache<root::smart_object*> dirty_render;
    line_cache<root::smart_object*> destroy_render;
    std::vector<std::shared_ptr<root::smart_object>> deferred_release;

    // Audio intents emitted by the model (emitters), drained each frame by
    // audio_bridge. A plain vector, not a line_cache: line_cache only holds
    // smart_object pointers, and these are PODs (see core/audio_message.h).
    std::vector<audio_message> audio_messages;

    void
    drop_pending()
    {
        dirty_transforms.clear();
        dirty_render.clear();
        destroy_render.clear();
        deferred_release.clear();
        audio_messages.clear();
    }
};

}  // namespace core
}  // namespace kryga

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

// The model's per-frame message boundary to other subsystems (main thread only),
// owned by model_system and reached via getr_model().output: POD intents the
// subsystem bridges drain each frame. Model-internal dirty/destroy bookkeeping is
// NOT here anymore — it moved to model_dirty.h (private to model_system, reached via
// getr_model().dirty()). The render command queue is a separate render-side
// subsystem — see vulkan_render/input_queue.h (render::input_queue).
struct alignas(k_cache_line) model_output
{
    // Audio intents emitted by the model (emitters), drained each frame by
    // audio_bridge. A plain vector, not a line_cache: line_cache only holds
    // smart_object pointers, and these are PODs (see core/audio_message.h).
    std::vector<audio_message> audio_messages;

    void
    drop_pending()
    {
        audio_messages.clear();
    }
};

}  // namespace core
}  // namespace kryga

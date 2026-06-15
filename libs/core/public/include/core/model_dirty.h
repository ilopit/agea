#pragma once

#include <core/caches/line_cache.h>

#include <packages/root/model/components/game_object_component.h>

#include <memory>
#include <vector>

namespace kryga
{
namespace core
{

// Model-INTERNAL dirty bookkeeping: "what have I not reconciled to the subsystems
// yet." Owned privately by model_system (m_dirty), produced through its queue_*
// methods, drained by the frame owner and read by level rollback/scrub via
// getr_model().dirty(). This is NOT the model->subsystem boundary — that is
// subsystem_queues (getr_subsystem_queues(): render command + audio message channels).
// Kept separate on purpose: dirty tracking is the model's own state, not an interface
// other subsystems should see.
struct alignas(64) model_dirty
{
    line_cache<root::game_object_component*> dirty_transforms;
    line_cache<root::smart_object*> dirty_render;
    line_cache<root::smart_object*> destroy_render;
    std::vector<std::shared_ptr<root::smart_object>> deferred_release;

    void
    clear()
    {
        dirty_transforms.clear();
        dirty_render.clear();
        destroy_render.clear();
        deferred_release.clear();
    }
};

}  // namespace core
}  // namespace kryga

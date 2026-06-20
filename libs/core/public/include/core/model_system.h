#pragma once

#include <core/caches/cache_set.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <core/id_generator.h>
#include <core/model_dirty.h>
#include <core/reflection/reflection_type.h>

#include <global_state/system.h>

namespace kryga::core
{

class model_system : public gs::system
{
public:
    std::string_view
    name() const override
    {
        return "model";
    }
    std::span<const std::string_view>
    deps() const override
    {
        return {};
    }

    void
    on_init(gs::state&) override
    {
        packages.init();
    }

    cache_set caches;

    reflection::reflection_type_registry reflection;
    id_generator id_gen;
    level_manager levels;
    package_manager packages;

    level* current_level = nullptr;

    // ---- Model-internal dirty bookkeeping (model_dirty.h) ----
    // Enqueue through these; never reach m_dirty directly to mark. The frame-owner
    // drain and level rollback/scrub read the lists through dirty().
    void
    queue_transform_dirty(root::game_object_component* c)
    {
        m_dirty.dirty_transforms.emplace_back(c);
    }
    void
    queue_render_dirty(root::smart_object* o)
    {
        m_dirty.dirty_render.emplace_back(o);
    }
    void
    queue_destroy_render(root::smart_object* o)
    {
        m_dirty.destroy_render.emplace_back(o);
    }

    // Frame-drain + level-internal reconcile (rollback/scrub) access. NOT for marking.
    model_dirty&
    dirty()
    {
        return m_dirty;
    }

    // Level teardown: clear the model-internal dirty bookkeeping. Pending model->
    // subsystem intents live on subsystem_queues, which self-clean: the render
    // channel rewinds on the render thread, and the audio channel is cancelled via
    // audio_translator::reap_orphans (stop intents) — no teardown drain is needed here.
    void
    drop_pending()
    {
        m_dirty.clear();
    }

private:
    model_dirty m_dirty;
};

}  // namespace kryga::core

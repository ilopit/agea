#pragma once

#include <core/caches/cache_set.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <core/screen_manager.h>
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
    screen_manager screens;  // live UI screens, peer to levels/packages

    level* current_level = nullptr;

    // ---- Model-internal dirty bookkeeping (model_dirty.h) ----
    // Enqueue through these; never reach m_dirty directly to mark. The frame-owner
    // drain and level rollback/scrub read the lists through dirty().
    void
    queue_transform_dirty(root::game_object_component* c)
    {
        m_dirty.dirty_transforms.emplace_back(c);
        invalidate_pick_index();
    }
    void
    queue_render_dirty(root::smart_object* o)
    {
        m_dirty.dirty_render.emplace_back(o);
        invalidate_pick_index();
    }
    void
    queue_destroy_render(root::smart_object* o)
    {
        m_dirty.destroy_render.emplace_back(o);
        invalidate_pick_index();
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
    // audio_translator::on_frame (orphan stop intents) — no teardown drain needed here.
    void
    drop_pending()
    {
        m_dirty.clear();
    }

private:
    // Any geometry/membership mutation invalidates the current level's pick index;
    // the next pick rebuilds it. Cheap (sets a flag), coarse on purpose — over-
    // invalidation only costs one rebuild on the next pick, a stale index would
    // mis-pick.
    void
    invalidate_pick_index()
    {
        if (current_level)
        {
            current_level->mark_pick_index_dirty();
        }
    }

    model_dirty m_dirty;
};

}  // namespace kryga::core

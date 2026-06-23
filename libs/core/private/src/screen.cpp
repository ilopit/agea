#include "core/screen.h"

#include "core/object_load_context_builder.h"
#include "core/object_constructor.h"
#include "core/model_system.h"

#include <packages/root/model/ui_widget.h>

#include <global_state/global_state.h>

namespace kryga::core
{

screen::screen(const utils::id& id)
    : container(id)
{
    // Bare context: local set + ownable cache only. No vfs mount (nothing is
    // loaded from disk), no level/package (the propagated pointers stay null;
    // ui_widgets never use them).
    m_occ = object_load_context_builder().set_from_container(*this).build();
}

screen::~screen()
{
    // Teardown is explicit (screen_manager::pop), mirroring level. The destructor
    // only frees the owned widgets via m_objects unwinding — at that point the
    // render thread is already gone, so no destroy_render queueing is needed.
}

root::smart_object*
screen::find_widget(const utils::id& id)
{
    return m_local_cs.objects.get_item(id);
}

root::smart_object*
screen::spawn_widget_impl(const utils::id& type_id,
                          const utils::id& id,
                          const root::smart_object::construct_params& prms)
{
    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto result = ctor.construct_obj(type_id, id, prms, false);
    if (!result)
    {
        return nullptr;
    }

    // ui_widgets are standalone smart_objects, render-built directly (the
    // game_object spawn path instead queues the root component). The property
    // setters that follow on the caller are no-ops on a freshly-constructed
    // object, so the first build must be queued here.
    auto* obj = result.value();
    glob::glob_state().getr_model().queue_render_dirty(obj);
    return obj;
}

void
screen::tick(float dt)
{
    // Every object a screen owns is a ui_widget (no sub-object hierarchy), so tick
    // m_objects directly — no tickable sub-list needed as in level. on_tick lives
    // on ui_widget (a root-package base type), not smart_object, so the cast is
    // the contract: a non-widget in a screen is a bug.
    for (auto& obj : m_objects)
    {
        obj->asr<root::ui_widget>().on_tick(dt);
    }
}

void
screen::unload()
{
    auto& model = glob::glob_state().getr_model();
    auto& deferred = model.dirty().deferred_release;

    for (auto& obj : m_objects)
    {
        model.queue_destroy_render(obj.get());
    }

    container::unregister_in_global_cache(m_local_cs, model.caches, m_id, "screen");

    // Drop ownership but keep each widget's shared_ptr alive until the render
    // thread drains destroy_render (it holds raw pointers). Reverse swap_and_remove
    // keeps indices valid.
    m_local_cs.clear();
    for (size_t i = m_objects.size(); i-- > 0;)
    {
        deferred.emplace_back(std::move(m_objects[i]));
        m_objects.swap_and_remove(m_objects.begin() + i);
    }
}

}  // namespace kryga::core

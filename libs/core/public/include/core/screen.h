#pragma once

#include "core/container.h"

#include <packages/root/model/smart_object.h>

#include <utils/id.h>

namespace kryga::core
{

// Runtime container for live UI widgets (root::ui_widget smart_objects). A model
// container, peer to level and package — owned by model_system (screens), so it
// sits alongside levels/packages rather than being tied to the play session.
//
// Pure-runtime: the load context carries NO vfs mount, NO level, NO package.
// Widget definitions still live in packages; here they are constructed
// imperatively (spawn_widget) — construction resolves the type from the global
// reflection registry, and this container only owns + registers the instances.
class screen : public container
{
public:
    explicit screen(const utils::id& id);
    ~screen();

    // Construct a widget instance and queue its first render build. T must be a
    // root::ui_widget (a smart_object, NOT a game_object) — mirrors
    // level::spawn_object but render-queues the object itself (there is no
    // game_object root component).
    template <typename T>
    T*
    spawn_widget(const utils::id& id, const typename T::construct_params& prms)
    {
        return spawn_widget_impl(T::AR_TYPE_id(), id, prms)->template as<T>();
    }

    root::smart_object*
    find_widget(const utils::id& id);

    // Per-frame update: ticks every owned widget (root::ui_widget::on_tick).
    // Animated widgets override on_tick and drive their own property changes.
    void
    tick(float dt);

    // Render-safe teardown: queue render-destroy for every owned widget,
    // unregister them from the global cache, then drop ownership (keeping each
    // shared_ptr alive until the render thread drains destroy_render).
    void
    unload();

private:
    root::smart_object*
    spawn_widget_impl(const utils::id& type_id,
                      const utils::id& id,
                      const root::smart_object::construct_params& prms);
};

}  // namespace kryga::core

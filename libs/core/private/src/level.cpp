#include "core/level.h"

#include <unordered_set>

#include <packages/root/model/game_object.h>

#include <core/caches/caches_map.h>
#include <core/model_system.h>
#include <core/object_load_context_builder.h>
#include <core/object_constructor.h>
#include <core/construction_utils.h>
#include <core/reflection/reflection_type.h>
#include <core/queues.h>
#include <global_state/global_state.h>

namespace kryga
{

namespace core
{

level::level(const utils::id& id)
    : container(id)
{
    m_occ = object_load_context_builder()
                .set_local_set(&m_local_cs)
                .set_ownable_cache(&m_objects)
                .set_level(this)
                .build();
}

level::~level()
{
}

root::game_object*
level::find_game_object(const utils::id& id)
{
    return m_local_cs.game_objects.get_item(id);
}

root::component*
level::find_component(const utils::id& id)
{
    return m_local_cs.components.get_item(id);
}

root::smart_object*
level::find_object(const utils::id& id)
{
    if (auto* go = find_game_object(id))
    {
        return go;
    }
    if (auto* c = find_component(id))
    {
        return c;
    }
    return nullptr;
}

root::smart_object*
level::spawn_object_impl(const utils::id& proto_id,
                         const utils::id& object_id,
                         const spawn_parameters& prms)
{
    auto proto_obj = m_occ->find_obj(proto_id);

    if (!proto_obj)
    {
        return nullptr;
    }

    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto inst_result = ctor.instantiate_obj(*proto_obj, object_id);
    if (!inst_result)
    {
        return nullptr;
    }
    auto result = inst_result.value();

    m_occ->reset_loaded_objects();

    auto obj = result->as<root::game_object>();

    obj->update_root();

    if (prms.position)
    {
        obj->set_position(prms.position.value());
    }
    if (prms.rotation)
    {
        obj->set_rotation(prms.rotation.value());
    }
    if (prms.scale)
    {
        obj->set_scale(prms.scale.value());
    }

    glob::glob_state().getr_queues().get_model().dirty_render.emplace_back(
        obj->get_root_component());

    m_tickable_objects.emplace_back(obj);

    return result;
}

root::smart_object*
level::spawn_object_as_clone_impl(const utils::id& proto_id,
                                  const utils::id& id,
                                  const spawn_parameters& prms)
{
    auto obj_to_clone = m_occ->find_obj(proto_id);

    if (!obj_to_clone)
    {
        return nullptr;
    }

    KRG_check(obj_to_clone->get_flags().instance_obj, "Should be always instance");

    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto clone_result = ctor.clone_obj(*obj_to_clone, id);
    if (!clone_result)
    {
        return nullptr;
    }
    auto result = clone_result.value();

    m_occ->reset_loaded_objects();

    auto obj = result->as<root::game_object>();

    obj->update_root();

    if (prms.position)
    {
        obj->set_position(prms.position.value());
    }
    if (prms.rotation)
    {
        obj->set_rotation(prms.rotation.value());
    }
    if (prms.scale)
    {
        obj->set_scale(prms.scale.value());
    }

    glob::glob_state().getr_queues().get_model().dirty_render.emplace_back(
        obj->get_root_component());

    m_tickable_objects.emplace_back(obj);

    return result;
}

root::smart_object*
level::spawn_object_impl(const utils::id& proto_id,
                         const utils::id& object_id,
                         const root::smart_object::construct_params& p)
{
    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto result = ctor.construct_obj(proto_id, object_id, p, false);
    if (!result)
    {
        return nullptr;
    }

    auto obj = result.value()->as<root::game_object>();
    if (obj)
    {
        glob::glob_state().getr_queues().get_model().dirty_render.emplace_back(
            obj->get_root_component());

        m_tickable_objects.emplace_back(obj);
    }

    return result.value();
}

void
level::destroy_game_object(root::game_object& go)
{
    for (auto* comp : go.get_renderable_components())
    {
        glob::glob_state().getr_queues().get_model().destroy_render.emplace_back(comp);
    }

    auto it = m_tickable_objects.find(&go);
    if (it != m_tickable_objects.end())
    {
        m_tickable_objects.swap_and_remove(it);
    }

    // Unregister AND drop ownership of the whole subtree (components + the
    // game_object). Components are entries in m_objects too, so removing only the
    // game_object would orphan them in the ownable vector. Collect first;
    // swap_and_remove reorders.
    std::unordered_set<root::smart_object*> to_drop;
    for (auto* comp : go.get_subcomponents())
    {
        m_occ->remove_obj(*comp);
        to_drop.insert(comp);
    }
    m_occ->remove_obj(go);
    to_drop.insert(&go);

    // Destroying a SURVIVOR during play (snapshot active): hold its shared_ptrs in
    // m_pending_destroy — destruction is deferred to the session outcome so rollback
    // can revive the intact graph with all ids/structure preserved. Otherwise (edit
    // mode, or a play-spawned object) free via deferred_release, which keeps the
    // shared_ptrs alive for the queued destroy_render raw pointers until the next
    // render drain clears it.
    bool pending_destroy = m_snapshot_all_ids.contains(go.get_id());
    auto& deferred = glob::glob_state().getr_queues().get_model().deferred_release;
    for (size_t i = m_objects.size(); i-- > 0;)
    {
        if (to_drop.contains(m_objects[i].get()))
        {
            (pending_destroy ? m_pending_destroy : deferred).emplace_back(std::move(m_objects[i]));
            m_objects.swap_and_remove(m_objects.begin() + i);
        }
    }
}

void
level::snapshot()
{
    // Capture pre-play property values for every per-play (non-readonly) object —
    // game_objects AND their components, since both are entries in m_objects. The
    // holder is a bare instance of the same type, allocated directly (never added
    // to a cache/occ) so it can't collide with the live object's id.
    m_snapshot.clear();
    m_snapshot_all_ids.clear();
    m_pending_destroy.clear();

    for (auto& obj : m_objects)
    {
        m_snapshot_all_ids.insert(obj->get_id());

        // Class-default/shared objects are not per-play state, and restoring into
        // a readonly object would assert. Skip them (but they ARE in all_ids, so
        // rollback won't mistake them for spawned-during-play).
        if (obj->get_flags().readonly)
        {
            continue;
        }

        auto id = obj->get_id();
        reflection::type_context__alloc alloc_ctx{&id};
        auto holder = obj->get_reflection()->alloc(alloc_ctx);

        snapshot_object_properties(*obj, *holder);
        m_snapshot[id] = std::move(holder);
    }
}

void
level::rollback()
{
    auto& q = glob::glob_state().getr_queues().get_model();
    auto& deferred = q.deferred_release;

    // Revive survivors parked by a mid-play destroy: re-register the intact graph
    // (back into m_objects + caches, all ids preserved), re-add game_objects to the
    // tick set, and queue a render rebuild (their GPU was freed on destroy and the
    // state is 'constructed', so mark_render_dirty would no-op — queue explicitly).
    // From here they are ordinary survivors: phase 1 cleans any pre-destroy graft,
    // phase 2 resets their values. m_snapshot holders still reference these same
    // (never-freed) component objects, so the restore stays pointer-valid.
    std::vector<root::game_object*> revived;
    for (auto& obj : m_pending_destroy)
    {
        if (auto* go = obj->as<root::game_object>())
        {
            revived.push_back(go);
        }
        m_occ->add_obj(std::move(obj));
    }
    m_pending_destroy.clear();
    for (auto* go : revived)
    {
        m_tickable_objects.emplace_back(go);
        q.dirty_render.emplace_back(go->get_root_component());
    }

    // Spawned-during-play = present in m_objects now but absent at snapshot. Keyed
    // by id, not index: a survivor destroyed mid-play reorders m_objects (destroy
    // uses swap_and_remove), so the old count-based [snapshot_count, end) range is
    // invalid. Readonly objects present at snapshot are in m_snapshot_all_ids and
    // so are NOT treated as spawned (must not be freed — survivors reference them).
    std::unordered_set<root::smart_object*> rolledback;
    for (auto& obj : m_objects)
    {
        if (!m_snapshot_all_ids.contains(obj->get_id()))
        {
            rolledback.insert(obj.get());
        }
    }

    // Scrub the pending main-thread queues of anything we're about to free. The
    // drain runs destroy_render first and releases deferred_release, then walks
    // dirty_render / dirty_transforms — so a stale entry there would build or
    // transform an already-freed object (use-after-free, and re-creates GPU
    // resources for a dead object). Same thread as the drain, so this is safe.
    auto scrub = [&](auto& cache)
    {
        for (size_t i = 0; i < cache.size();)
        {
            if (rolledback.contains(cache[i]))
            {
                cache.swap_and_remove(cache.begin() + i);
            }
            else
            {
                ++i;
            }
        }
    };
    scrub(q.dirty_render);
    scrub(q.dirty_transforms);

    // Phase 1 — tear down spawned objects. Detach uses the precomputed rolledback
    // set, so processing order doesn't affect correctness; deferred_release keeps
    // every freed shared_ptr alive until the render thread drains destroy_render.
    for (auto& obj : m_objects)
    {
        if (!rolledback.contains(obj.get()))
        {
            continue;
        }

        // Skip shared class/package objects (readonly) for render-destroy/detach:
        // a CDO (or its class-derived sub-object) loaded on first use mid-play has
        // no instance render resources, must never be render-destroyed (render_bridge
        // asserts !default_obj, and tearing into shared state would corrupt
        // survivors that reference it), and has no per-play parent to detach.
        if (auto* comp = obj->as<root::component>(); comp && !comp->get_flags().readonly)
        {
            // Only game_object_components carry GPU render data.
            if (auto* goc = comp->as<root::game_object_component>())
            {
                q.destroy_render.emplace_back(goc);
            }

            // Orphan case: a subtree grafted at runtime onto a component that
            // survives the rollback. Detach only the subtree root (the one node
            // whose parent survives) — recreate_structure_from_layout then drops
            // its whole subtree from the surviving parent in a single rebuild, so
            // inner nodes (parent also rolled back) need no handling. Components
            // of a fully rolled-back object have a rolled-back parent too and are
            // freed wholesale.
            auto* parent = comp->get_parent();
            if (parent && !rolledback.contains(parent))
            {
                comp->get_owner()->detach(comp);
            }
        }

        if (auto go = obj->as<root::game_object>())
        {
            auto it = m_tickable_objects.find(go);
            if (it != m_tickable_objects.end())
            {
                m_tickable_objects.swap_and_remove(it);
            }
        }

        m_occ->remove_obj(*obj);
    }

    // Physically remove the spawned objects from m_objects, moving each into
    // deferred_release (keeps the shared_ptr alive for the queued destroy_render
    // raw pointers). Reverse swap_and_remove so indices stay valid.
    for (size_t i = m_objects.size(); i-- > 0;)
    {
        if (rolledback.contains(m_objects[i].get()))
        {
            deferred.emplace_back(std::move(m_objects[i]));
            m_objects.swap_and_remove(m_objects.begin() + i);
        }
    }

    // Phase 2 — restore surviving objects to their pre-play property values.
    // Phase 1 has removed spawned objects and structurally un-grafted any runtime
    // subtrees, so each survivor now matches its snapshot's structure; copy the
    // captured values back in place (object identity preserved — no realloc).
    for (auto& obj : m_objects)
    {
        auto it = m_snapshot.find(obj->get_id());
        if (it == m_snapshot.end())
        {
            continue;  // readonly/CDO: never snapshotted, nothing to restore
        }
        snapshot_object_properties(*it->second, *obj);  // holder -> live
    }

    // Re-sync the GPU for restored survivors. update_position recomputes each
    // game_object's transform hierarchy from the restored model values; the
    // renderable components are then marked render-dirty so the render thread
    // runs a full render_cmd_build, re-collecting BOTH the transform and any
    // restored render-only properties (material, visibility, mesh, ...). A
    // transform-only re-sync would miss those non-transform props. render_cmd_build
    // re-collects gpu data into the existing cache slots — no buffer/identity
    // teardown — so the toggle-latency win (no destroy/rebuild of survivors) is
    // kept. mark_render_dirty queues because a survivor is render_ready (not
    // constructed); the dirty_render drain then rebuilds it.
    for (auto& obj : m_objects)
    {
        if (!m_snapshot.contains(obj->get_id()))
        {
            continue;
        }
        if (auto* go = obj->as<root::game_object>())
        {
            go->update_position();
            for (auto* goc : go->get_renderable_components())
            {
                goc->mark_render_dirty();
            }
        }
    }

    // Survivors destroyed during play were already revived at the top of rollback
    // (re-registered intact and reset by phases 1-2), so there is no separate
    // recreate step.

    m_snapshot.clear();
    m_snapshot_all_ids.clear();
}

void
level::tick(float dt)
{
    for (auto o : m_tickable_objects)
    {
        o->on_tick(dt);
        for (auto c : o->get_renderable_components())
        {
            c->on_tick(dt);
        }
    }
}

void
level::unregister_objects()
{
    container::unregister_in_global_cache(
        m_local_cs, glob::glob_state().getr_model().caches, m_id, "level");
}

void
level::unload()
{
    container::unload();

    m_tickable_objects.clear();
    m_package_ids.clear();
    clear_lightmap();

    m_state = level_state::unloaded;
}

void
level::set_lightmap(uint32_t bindless_index, std::unique_ptr<lightmap_manifest> manifest)
{
    m_lightmap_bindless_index = bindless_index;
    m_lightmap_manifest = std::move(manifest);
}

void
level::clear_lightmap()
{
    m_lightmap_bindless_index = 0xFFFFFFFFu;
    m_lightmap_manifest.reset();
}

}  // namespace core
}  // namespace kryga

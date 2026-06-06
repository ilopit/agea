#pragma once

#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"
#include "core/model_minimal.h"
#include "core/container.h"
#include "core/lightmap_manifest.h"

namespace kryga::vfs
{
class backend;
}

#include <packages/root/model/core_types/vec3.h>

#include <utils/singleton_instance.h>

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace kryga
{
namespace core
{
struct spawn_parameters
{
    std::optional<root::vec3> position;
    std::optional<root::vec3> scale;
    std::optional<root::vec3> rotation;
};

enum class level_state
{
    unloaded = 0,
    loaded,
    render_loaded
};

class level : public container
{
public:
    friend class level_manager;

    level(const utils::id& id);
    ~level();

    root::game_object*
    find_game_object(const utils::id& id);

    root::component*
    find_component(const utils::id& id);

    root::smart_object*
    find_object(const utils::id& id);

    template <typename T>
    auto
    spawn_object_from_proto(const utils::id& proto_id,
                            const utils::id& id,
                            const spawn_parameters& prms)
    {
        return spawn_object_impl(proto_id, id, prms)->as<T>();
    }

    template <typename T>
    auto
    spawn_object_as_clone(const utils::id& proto_id,
                          const utils::id& id,
                          const spawn_parameters& prms)
    {
        return spawn_object_as_clone_impl(proto_id, id, prms)->template as<T>();
    }

    template <typename T>
    auto
    spawn_object(const utils::id& id, const typename T::construct_params& prms)
    {
        return spawn_object_impl(T::AR_TYPE_id(), id, prms)->template as<T>();
    }

    void
    destroy_game_object(root::game_object& go);

    void
    snapshot();

    void
    rollback();

    void
    tick(float dt);

    game_objects_cache&
    get_game_objects()
    {
        return m_local_cs.game_objects;
    }

    const std::vector<utils::id>&
    get_package_ids() const
    {
        return m_package_ids;
    }

    void
    add_package_id(const utils::id& id)
    {
        m_package_ids.push_back(id);
    }

    object_load_context&
    get_load_context() const
    {
        return *m_occ;
    }

    void
    unregister_objects();

    void
    unload();

    level_state
    get_state() const
    {
        return m_state;
    }

private:
    root::smart_object*
    spawn_object_impl(const utils::id& proto_id, const utils::id& id, const spawn_parameters& prms);

    root::smart_object*
    spawn_object_as_clone_impl(const utils::id& proto_id,
                               const utils::id& id,
                               const spawn_parameters& prms);

    root::smart_object*
    spawn_object_impl(const utils::id& proto_id,
                      const utils::id& id,
                      const root::smart_object::construct_params& p);

    level_state m_state = level_state::unloaded;
    vfs::backend* m_backend = nullptr;

    line_cache<root::game_object*> m_tickable_objects;

    // Pre-play property snapshot, keyed by object id (NOT pointer: a survivor can
    // be destroyed mid-play and its address reused by a later spawn — id is stable
    // and ABA-safe). m_snapshot holds a bare, unregistered holder per non-readonly
    // object with its pre-play property values; m_snapshot_all_ids is every object
    // present at snapshot (incl. readonly) so rollback can tell spawned-during-play
    // (free) from present-at-snapshot (keep). See docs/plans/play-mode-state-snapshot.md.
    std::unordered_map<utils::id, std::shared_ptr<root::smart_object>> m_snapshot;
    std::unordered_set<utils::id> m_snapshot_all_ids;

    // Editor play-mode only. A survivor destroyed during play is held here with
    // its destruction PENDING the session outcome — the whole object graph is kept
    // alive (just unregistered + un-rendered) rather than freed, so rollback() can
    // re-add it and reset it like any survivor, identity and all ids preserved.
    // (Game builds never enter play mode, so this stays empty there.)
    std::vector<std::shared_ptr<root::smart_object>> m_pending_destroy;

    std::vector<utils::id> m_package_ids;

    utils::id m_selected_directional_light_id;

    // Baked lighting — paths read from root.cfg. The runtime lightmap binding
    // (atlas bindless index + per-object UVs) is owned render-side in the loader's
    // per-level registry, NOT cached here: the texture is created on the render
    // thread on load, so the model holds only the source rids.
    vfs::rid m_lightmap_bin_rid;  // empty = no lightmap
    vfs::rid m_lightmap_manifest_rid;

public:
    void
    set_selected_directional_light(const utils::id& id)
    {
        m_selected_directional_light_id = id;
    }

    const utils::id&
    get_selected_directional_light_id() const
    {
        return m_selected_directional_light_id;
    }

    // Lightmap references (from root.cfg, set by baker)
    bool
    has_lightmap_ref() const
    {
        return !m_lightmap_bin_rid.empty();
    }
    const vfs::rid&
    get_lightmap_bin_rid() const
    {
        return m_lightmap_bin_rid;
    }
    const vfs::rid&
    get_lightmap_manifest_rid() const
    {
        return m_lightmap_manifest_rid;
    }
    void
    set_lightmap_refs(const vfs::rid& bin, const vfs::rid& manifest)
    {
        m_lightmap_bin_rid = bin;
        m_lightmap_manifest_rid = manifest;
    }
};

}  // namespace core

}  // namespace kryga

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

#include <spatial/object_bvh.h>

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

    // Nearest game_object whose renderable world-sphere the cursor ray hits, via the
    // model-side spatial index. Builds the world ray from the supplied camera matrices.
    // Returns nullptr on miss. Replaces the per-pick whole-scene scan. Model thread.
    root::game_object*
    pick_under_cursor(const glm::mat4& inv_projection,
                      const glm::mat4& view,
                      int32_t mouse_x,
                      int32_t mouse_y,
                      uint32_t screen_w,
                      uint32_t screen_h);

    // Invalidate the pick index — the next pick rebuilds it. Cheap (sets a flag);
    // called from model_system on any transform / render / destroy mutation.
    void
    mark_pick_index_dirty()
    {
        m_pick_index_dirty = true;
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

    // The active camera in this level, by id (not pointer: address reuse makes raw
    // pointers unsafe — see the snapshot note above). Runtime state written through
    // camera_component::set_active_camera; resolved O(1) via find_component. Empty
    // until first set/registration. Replaces a per-frame whole-scene scan.
    utils::id m_active_camera_id;

    // Model-side spatial index over renderable world-bounds, for picking. Lazily
    // (re)built on the next pick when m_pick_index_dirty — invalidated by model_system
    // on transform / render / destroy mutations. Indexes real scene objects only;
    // editor-only icon proxies live render-side (kryga_render object BVH).
    spatial::object_bvh m_pick_index;
    bool m_pick_index_dirty = true;

    void
    rebuild_pick_index();

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

    void
    set_active_camera_id(const utils::id& id)
    {
        m_active_camera_id = id;
    }

    const utils::id&
    get_active_camera_id() const
    {
        return m_active_camera_id;
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

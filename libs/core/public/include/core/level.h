#pragma once

#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"
#include "core/objects_mapping.h"
#include "core/model_minimal.h"
#include "core/container.h"

#include <packages/root/components/mesh_component.h>
#include <packages/root/core_types/vec3.h>

#include <utils/singleton_instance.h>

#include <map>
#include <vector>
#include <string>
#include <optional>

namespace agea
{
namespace core
{
struct spawn_parameters
{
    std::optional<root::vec3> positon;
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

    level(const utils::id& id, cache_set* class_global_set, cache_set* instance_global_set);
    ~level();

    root::game_object*
    find_game_object(const utils::id& id);

    root::component*
    find_component(const utils::id& id);

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
    spawn_object(const utils::id& id, const typename T::construct_params& prms)
    {
        return spawn_object_impl(T::AR_TYPE_id(), id, prms)->as<T>();
    }

    void
    tick(float dt);

    game_objects_cache&
    get_game_objects()
    {
        return *m_instance_local_cs.game_objects.get();
    }

    void
    add_to_dirty_transform_queue(root::game_object_component* g)
    {
        AGEA_check(g, "Should not be NULL");
        m_dirty_transform_components.emplace_back(g);
    }

    line_cache<root::game_object_component*>&
    get_dirty_transforms_components_queue()
    {
        return m_dirty_transform_components;
    }

    void
    add_to_dirty_render_queue(root::game_object_component* g)
    {
        AGEA_check(g, "Should not be NULL");
        m_dirty_render_components.emplace_back(g);
    }

    line_cache<root::game_object_component*>&
    get_dirty_render_queue()
    {
        return m_dirty_render_components;
    }

    void
    add_to_dirty_render_assets_queue(root::asset* a)
    {
        AGEA_check(a, "Should not be NULL");
        m_dirty_render_assets.emplace_back(a);
    }

    line_cache<root::asset*>&
    get_dirty_render_assets_queue()
    {
        return m_dirty_render_assets;
    }

    void
    add_to_dirty_shader_effect_queue(root::shader_effect* se)
    {
        AGEA_check(se, "Should not be NULL");
        m_dirty_shader_effects.emplace_back(se);
    }

    line_cache<root::shader_effect*>&
    get_dirty_shader_effect_queue()
    {
        return m_dirty_shader_effects;
    }

    const std::vector<utils::id>&
    get_package_ids() const
    {
        return m_package_ids;
    }

    object_load_context&
    get_load_context() const
    {
        return *m_occ;
    }

    void
    drop_pending_updates();

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
    spawn_object_impl(const utils::id& proto_id,
                      const utils::id& id,
                      const root::smart_object::construct_params& p);

    level_state m_state = level_state::unloaded;

    line_cache<root::game_object*> m_tickable_objects;

    line_cache<root::game_object_component*> m_dirty_transform_components;
    line_cache<root::game_object_component*> m_dirty_render_components;
    line_cache<root::asset*> m_dirty_render_assets;
    line_cache<root::shader_effect*> m_dirty_shader_effects;

    std::vector<utils::id> m_package_ids;
};

}  // namespace core

namespace glob
{
class level : public ::agea::singleton_instance<::agea::core::level, level>
{
};
}  // namespace glob
}  // namespace agea

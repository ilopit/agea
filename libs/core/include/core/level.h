#pragma once

#include "root/components/mesh_component.h"

#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"
#include "core/objects_mapping.h"
#include "core/model_minimal.h"

#include <root/core_types/vec3.h>

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

class level
{
public:
    friend class level_manager;

    level();
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

    const utils::path&
    get_load_path() const
    {
        return m_load_path;
    }

    const utils::path&
    get_save_root_path() const
    {
        return m_save_root_path;
    }

    void
    set_load_path(const utils::path& path)
    {
        m_load_path = path;
    }

    void
    set_save_root_path(const utils::path& path)
    {
        m_save_root_path = path;
    }

    game_objects_cache&
    get_game_objects()
    {
        return *m_local_cs.game_objects.get();
    }

    const utils::id&
    get_id()
    {
        return m_id;
    }

    void
    add_to_dirty_transform_queue(root::game_object_component* g)
    {
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
    clear();

    cache_set&
    get_local_cs()
    {
        return m_local_cs;
    }

private:
    root::smart_object*
    spawn_object_impl(const utils::id& proto_id, const utils::id& id, const spawn_parameters& prms);

    root::smart_object*
    spawn_object_impl(const utils::id& proto_id,
                      const utils::id& id,
                      const root::smart_object::construct_params& p);

    utils::id m_id;

    cache_set m_local_cs;
    cache_set* m_global_object_cs = nullptr;
    cache_set* m_global_class_object_cs = nullptr;
    std::unique_ptr<object_load_context> m_occ;

    line_cache<root::smart_object_ptr> m_objects;
    line_cache<root::game_object*> m_tickable_objects;

    line_cache<root::game_object_component*> m_dirty_transform_components;
    line_cache<root::game_object_component*> m_dirty_render_components;
    line_cache<root::asset*> m_dirty_render_assets;
    line_cache<root::shader_effect*> m_dirty_shader_effects;

    std::vector<utils::id> m_package_ids;

    std::shared_ptr<object_mapping> m_mapping;

    utils::path m_load_path;
    utils::path m_save_root_path;
};

}  // namespace core

namespace glob
{
class level : public ::agea::singleton_instance<::agea::core::level, level>
{
};
}  // namespace glob
}  // namespace agea

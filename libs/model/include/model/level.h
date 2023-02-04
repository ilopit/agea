#pragma once

#include "model/components/camera_component.h"
#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"
#include "model/objects_mapping.h"

#include "model/model_minimal.h"

#include <utils/singleton_instance.h>

#include <map>
#include <vector>
#include <string>

namespace agea
{
namespace model
{

class level
{
public:
    friend class level_manager;

    level();
    ~level();

    game_object*
    find_game_object(const utils::id& id);

    component*
    find_component(const utils::id& id);

    template <typename T>
    auto
    spawn_object(const utils::id& type_id, const utils::id& id)
    {
        return spawm_object(type_id, id)->as<T>();
    }

    smart_object*
    spawm_object(const utils::id& proto_obj, const utils::id& object_id);

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
    add_to_dirty_components_queue(game_object_component* g)
    {
        m_dirty_transform_components.emplace_back(g);
    }

    line_cache<game_object_component*>&
    get_dirty_transforms_components_queue()
    {
        return m_dirty_transform_components;
    }

    void
    add_to_dirty_render_queue(game_object_component* g)
    {
        m_dirty_render_components.emplace_back(g);
    }

    line_cache<game_object_component*>&
    get_dirty_render_queue()
    {
        return m_dirty_render_components;
    }

    void
    add_to_dirty_render_assets_queue(asset* a)
    {
        m_dirty_render_assets.emplace_back(a);
    }

    line_cache<asset*>&
    get_dirty_render_assets_queue()
    {
        return m_dirty_render_assets;
    }

    void
    add_to_dirty_shader_effect_queue(shader_effect* se)
    {
        m_dirty_shader_effects.emplace_back(se);
    }

    line_cache<shader_effect*>&
    get_dirty_shader_effect_queue()
    {
        return m_dirty_shader_effects;
    }

    const std::vector<utils::id>&
    get_package_ids() const
    {
        return m_package_ids;
    }

private:
    utils::id m_id;

    cache_set m_local_cs;
    cache_set* m_global_object_cs = nullptr;
    cache_set* m_global_class_object_cs = nullptr;
    std::unique_ptr<object_load_context> m_occ;

    line_cache<smart_object_ptr> m_objects;
    line_cache<game_object*> m_tickable_objects;

    line_cache<game_object_component*> m_dirty_transform_components;
    line_cache<game_object_component*> m_dirty_render_components;
    line_cache<asset*> m_dirty_render_assets;
    line_cache<shader_effect*> m_dirty_shader_effects;

    std::vector<utils::id> m_package_ids;

    std::shared_ptr<object_mapping> m_mapping;

    utils::path m_load_path;
    utils::path m_save_root_path;
};

}  // namespace model

namespace glob
{
class level : public ::agea::singleton_instance<::agea::model::level, level>
{
};
}  // namespace glob
}  // namespace agea

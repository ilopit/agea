#pragma once

#include "model/components/camera_component.h"
#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"

#include "utils/weird_singletone.h"

#include "model/model_minimal.h"

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
    friend class level_constructor;

    level();
    ~level();

    void
    construct();

    object_constructor_context&
    occ();

    game_object*
    find_game_object(const utils::id& id);

    smart_object*
    find_object(const utils::id& id);

    component*
    find_component(const utils::id& id);

    void
    update();

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

private:
    utils::id m_id;

    cache_set m_local_cs;
    cache_set_ref m_global_object_cs;
    cache_set_ref m_global_class_object_cs;
    std::unique_ptr<object_constructor_context> m_occ;

    line_cache m_objects;
    std::vector<utils::id> m_package_ids;

    utils::path m_load_path;
    utils::path m_save_root_path;
};

}  // namespace model

namespace glob
{
class level : public ::agea::selfcleanable_singleton<::agea::model::level>
{
};
}  // namespace glob
}  // namespace agea

#pragma once

#include "model/components/camera_component.h"
#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"

#include "utils/weird_singletone.h"

#include "core/agea_minimal.h"

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
    find_game_object(const core::id& id);

    smart_object*
    find_object(const core::id& id);

    component*
    find_component(const core::id& id);

    void
    update();

    game_objects_cache&
    get_game_objects()
    {
        return *m_local_cs.game_objects.get();
    }

private:
    cache_set m_local_cs;
    cache_set_ref m_global_object_cs;
    cache_set_ref m_global_class_object_cs;

    line_cache m_objects;
    std::vector<core::id> m_package_ids;

    std::unique_ptr<object_constructor_context> m_occ;
    utils::path m_path;
};

}  // namespace model

namespace glob
{
class level : public weird_singleton<::agea::model::level>
{
};
}  // namespace glob
}  // namespace agea

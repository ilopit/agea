#pragma once

#include "model/components/camera_component.h"
#include "model/model_fwds.h"
#include "model/caches/cache_set.h"

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
    level();
    ~level();

    void
    construct();

    object_constructor_context&
    occ();

    game_object*
    find_game_object(const std::string& id);

    smart_object*
    find_object(const std::string& id);

    component*
    find_component(const std::string& id);

    void
    update();

    cache_set m_local_cs;
    cache_set_ref m_global_cs;

    std::vector<std::shared_ptr<smart_object>> m_objects;
    std::vector<std::string> m_package_ids;

    std::unique_ptr<object_constructor_context> m_occ;
    std::string m_path;
};

}  // namespace model

namespace glob
{
class level : public weird_singleton<::agea::model::level>
{
};
}  // namespace glob
}  // namespace agea

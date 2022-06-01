#pragma once

#include "model/components/camera_component.h"
#include "model/model_fwds.h"

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

    camera_component*
    get_camera(const std::string& camers);

    game_object*
    find_game_object(const std::string& id);

    smart_object*
    find_object(const std::string& id);

    smart_object*
    find_component(const std::string& id);

    void
    update();

    std::unordered_map<std::string, game_object*> m_objects;
    std::unordered_map<std::string, component*> m_components;

    std::map<std::string, camera_component*> m_cameras;

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

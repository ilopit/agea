#pragma once

#include "level_object.h"
#include "model/components/camera_component.h"
#include "utils/weird_singletone.h"

#include "agea_minimal.h"

#include <map>
#include <vector>
#include <string>

namespace agea
{
namespace model
{
class player;
class level
{
public:
    void construct();

    camera_component* get_camera(const std::string& camers);

    level();

    ~level();

    template <typename object_type>
    object_type*
    spawn_object(typename object_type::construct_params& p)
    {
        auto obj = std::make_unique<object_type>();
        obj->META_construct(object_type::META_class_id(), "bla");
        obj->construct(p);

        auto result = obj.get();
        m_objects.push_back(std::move(obj));

        auto c = result->find_component<camera_component>();

        if (c)
        {
            m_cameras[c->name()] = (camera_component*)c;
        }

        return result;
    }

    template <typename component_type>
    component_type*
    spawn_component(typename component_type::construct_params& p)
    {
        auto obj = std::make_unique<component_type>();
        obj->META_construct(p);

        auto result = obj.get();
        // m_components.push_back(std::move(obj));

        return result;
    }

    static bool load(const std::string& name);

    void update();

    std::vector<std::shared_ptr<level_object>> m_objects;
    std::vector<component*> m_components;

    std::map<std::string, camera_component*> m_cameras;
    std::map<std::string, player*> m_avatars;
};

}  // namespace model

namespace glob
{
class level : public weird_singleton<::agea::model::level>
{
};
}  // namespace glob
}  // namespace agea

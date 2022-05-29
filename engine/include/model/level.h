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

    smart_object*
    find_object(const std::string& id);

    smart_object*
    find_component(const std::string& id);

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

        return result;
    }

    static bool
    load(const std::string& name);

    void
    update();

    std::vector<game_object*> m_objects;

    std::map<std::string, camera_component*> m_cameras;

    std::unique_ptr<object_constructor_context> m_occ;
};

}  // namespace model

namespace glob
{
class level : public weird_singleton<::agea::model::level>
{
};
}  // namespace glob
}  // namespace agea

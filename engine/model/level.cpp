#include "model/level.h"

#include "model/game_object.h"
#include "model/object_construction_context.h"
#include "model/caches/objects_cache.h"

namespace agea
{
namespace model
{
void
level::construct()
{
}

object_constructor_context&
level::occ()
{
    return *m_occ.get();
}

camera_component*
level::get_camera(const std::string& camers)
{
    if (m_cameras.empty())
    {
        return nullptr;
    }
    auto itr = m_cameras.find(camers);
    if (itr == m_cameras.end())
    {
        return m_cameras.begin()->second;
    }

    return itr->second;
}

game_object*
level::find_game_object(const std::string& id)
{
    auto itr = m_objects.find(id);

    return itr != m_objects.end() ? itr->second : nullptr;
}

agea::model::smart_object*
level::find_object(const std::string& id)
{
    return m_occ->instance_obj_cache->get(id).get();
}

smart_object*
level::find_component(const std::string& id)
{
    for (auto& o : m_objects)
    {
        for (auto c : o.second->get_components())
        {
            if (c->get_id() == id)
            {
                return c;
            }
        }
    }

    return nullptr;
}

void
level::update()
{
    for (auto& o : m_objects)
    {
        o.second->update();
    }
}

level::level()
    : m_occ(std::make_unique<object_constructor_context>())
{
}

level::~level()
{
}

}  // namespace model
}  // namespace agea

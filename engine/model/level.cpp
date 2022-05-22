#include "model/level.h"

#include "model/game_object.h"
#include "model/object_construction_context.h"

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

smart_object*
level::find_object(const std::string& id)
{
    auto iobj = std::find_if(m_objects.begin(), m_objects.end(),
                             [&id](model::game_object* obj) { return obj->id() == id; });

    return iobj != m_objects.end() ? *iobj : nullptr;
}

smart_object*
level::find_component(const std::string& id)
{
    for (auto& o : m_objects)
    {
        for (auto c : o->m_components)
        {
            if (c->id() == id)
            {
                return c;
            }
        }
    }

    return nullptr;
}

bool
level::load(const std::string&)
{
    return true;
}

void
level::update()
{
    for (auto& o : m_objects)
    {
        o->update();
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

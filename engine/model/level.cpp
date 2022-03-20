#include "model/level.h"

#include "model/level_object.h"

namespace agea
{
namespace model
{
void
level::construct()
{
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

bool
level::load(const std::string& name)
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
{
}

level::~level()
{
}

}  // namespace model
}  // namespace agea

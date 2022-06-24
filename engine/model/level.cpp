#include "model/level.h"

#include "model/game_object.h"
#include "model/object_construction_context.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/class_object_cache.h"

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

game_object*
level::find_game_object(const core::id& id)
{
    return m_local_cs.game_objects->get_item(id);
}

agea::model::smart_object*
level::find_object(const core::id& id)
{
    return nullptr;  // m_occ->get_item(id);
}

component*
level::find_component(const core::id& id)
{
    return m_local_cs.components->get_item(id);
}

void
level::update()
{
    for (auto& o : m_objects.get_items())
    {
        if (auto obj = o->as<game_object>())
        {
            obj->update();
        }
    }
}

level::level()
    : m_occ(nullptr)
{
}

level::~level()
{
}

}  // namespace model
}  // namespace agea

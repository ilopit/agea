#include "model/level.h"

#include "model/game_object.h"
#include "model/assets/asset.h"
#include "model/assets/shader_effect.h"

#include "model/caches/game_objects_cache.h"
#include "model/caches/caches_map.h"

#include "model/object_construction_context.h"

namespace agea
{

glob::level::type glob::level::type::s_instance;

namespace model
{

level::level()
    : m_occ(nullptr)
{
}

level::~level()
{
}

object_constructor_context&
level::occ()
{
    return *m_occ.get();
}

game_object*
level::find_game_object(const utils::id& id)
{
    return m_local_cs.game_objects->get_item(id);
}

component*
level::find_component(const utils::id& id)
{
    return m_local_cs.components->get_item(id);
}

void
level::tick(float dt)
{
    for (auto o : m_tickable_objects)
    {
        o->on_tick(dt);
        game_object::over_tickable(o->get_root_component(),
                                   [dt](game_object_component* obj) { obj->on_tick(dt); });
    }
}

}  // namespace model
}  // namespace agea

#include "model/level.h"

#include "model/game_object.h"
#include "model/assets/asset.h"
#include "model/assets/shader_effect.h"

#include "model/caches/game_objects_cache.h"
#include "model/caches/caches_map.h"

#include "model/object_load_context.h"
#include "model/object_constructor.h"

namespace agea
{

glob::level::type glob::level::type::s_instance;

namespace model
{

level::level()
    : m_occ(nullptr)
    , m_mapping(std::make_shared<model::object_mapping>())
{
}

level::~level()
{
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

smart_object*
level::spawm_object(const utils::id& proto_obj_id, const utils::id& object_id)
{
    auto proto_obj = m_occ->find_obj(proto_obj_id);

    if (!proto_obj)
    {
        return nullptr;
    }

    smart_object* result = nullptr;
    std::vector<smart_object*> loaded_obj;

    auto rc = object_constructor::object_clone(*proto_obj, object_id, *m_occ, result, loaded_obj);
    if (rc != result_code::ok)
    {
        return nullptr;
    }

    for (auto o : loaded_obj)
    {
        o->post_load();
        o->set_state(smart_object_state::constructed);
    }

    return result;
}

void
level::tick(float dt)
{
    for (auto o : m_tickable_objects)
    {
        o->on_tick(dt);
        for (auto c : o->get_renderable_components())
        {
            c->on_tick(dt);
        }
    }
}

}  // namespace model
}  // namespace agea

#include "core/level.h"

#include "root/game_object.h"
#include "root/assets/asset.h"
#include "root/assets/shader_effect.h"

#include "core/caches/game_objects_cache.h"
#include "core/caches/caches_map.h"

#include "core/object_load_context.h"
#include "core/object_constructor.h"

namespace agea
{

glob::level::type glob::level::type::s_instance;

namespace core
{

level::level()
    : m_occ(std::make_unique<object_load_context>())
    , m_mapping(std::make_shared<core::object_mapping>())
{
    m_occ->set_instance_local_set(&m_local_cs)
        .set_ownable_cache(&m_objects)
        .set_objects_mapping(m_mapping)
        .set_level(this);
}

level::~level()
{
}

root::game_object*
level::find_game_object(const utils::id& id)
{
    return m_local_cs.game_objects->get_item(id);
}

root::component*
level::find_component(const utils::id& id)
{
    return m_local_cs.components->get_item(id);
}

root::smart_object*
level::spawn_object_impl(const utils::id& proto_id,
                         const utils::id& object_id,
                         const spawn_parameters& prms)
{
    auto proto_obj = m_occ->find_obj(proto_id);

    if (!proto_obj)
    {
        return nullptr;
    }

    root::smart_object* result = nullptr;
    std::vector<root::smart_object*> loaded_obj;

    m_occ->set_construction_type(object_load_type::instance_obj);

    auto rc =
        object_constructor::object_clone_create_internal(*proto_obj, object_id, *m_occ, result);
    if (rc != result_code::ok)
    {
        return nullptr;
    }

    m_occ->reset_loaded_objects(loaded_obj);

    auto obj = result->as<root::game_object>();

    obj->update_root();

    if (prms.positon)
    {
        obj->set_position(prms.positon.value());
    }
    if (prms.rotation)
    {
        obj->set_rotation(prms.rotation.value());
    }
    if (prms.scale)
    {
        obj->set_scale(prms.scale.value());
    }

    result->post_load();

    add_to_dirty_render_queue(obj->get_root_component());

    m_tickable_objects.emplace_back(obj);

    return result;
}

root::smart_object*
level::spawn_object_impl(const utils::id& proto_id,
                         const utils::id& object_id,
                         const root::smart_object::construct_params& p)
{
    auto obj = object_constructor::object_construct(proto_id, object_id, p, *m_occ)
                   ->as<root::game_object>();

    add_to_dirty_render_queue(obj->get_root_component());

    m_tickable_objects.emplace_back(obj);

    return obj;
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

void
level::drop_pending_updates()
{
    m_dirty_transform_components.clear();
    m_dirty_render_components.clear();
    m_dirty_render_assets.clear();
    m_dirty_shader_effects.clear();
}

}  // namespace core
}  // namespace agea

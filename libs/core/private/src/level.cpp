#include "core/level.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/asset.h>
#include <packages/root/model/assets/shader_effect.h>

#include <core/caches/caches_map.h>
#include <core/object_load_context_builder.h>
#include <core/object_constructor.h>
#include <core/queues.h>
#include <global_state/global_state.h>

namespace kryga
{

namespace core
{

level::level(const utils::id& id)
    : container(id)
{
    m_occ = object_load_context_builder()
                .set_instance_local_set(&m_instance_local_cs)
                .set_ownable_cache(&m_objects)
                .set_level(this)
                .build();
}

level::~level()
{
}

root::game_object*
level::find_game_object(const utils::id& id)
{
    return m_instance_local_cs.game_objects.get_item(id);
}

root::component*
level::find_component(const utils::id& id)
{
    return m_instance_local_cs.components.get_item(id);
}

root::smart_object*
level::spawn_object_impl(const utils::id& proto_id,
                         const utils::id& object_id,
                         const spawn_parameters& prms)
{
    auto proto_obj = m_occ->find_proto_obj(proto_id);

    if (!proto_obj)
    {
        return nullptr;
    }

    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto inst_result = ctor.instantiate_obj(*proto_obj, object_id);
    if (!inst_result)
    {
        return nullptr;
    }
    auto result = inst_result.value();

    m_occ->reset_loaded_objects();

    auto obj = result->as<root::game_object>();

    obj->update_root();

    if (prms.position)
    {
        obj->set_position(prms.position.value());
    }
    if (prms.rotation)
    {
        obj->set_rotation(prms.rotation.value());
    }
    if (prms.scale)
    {
        obj->set_scale(prms.scale.value());
    }

    glob::glob_state().getr_queues().get_model().dirty_render_components.emplace_back(
        obj->get_root_component());

    m_tickable_objects.emplace_back(obj);

    return result;
}

root::smart_object*
level::spawn_object_as_clone_impl(const utils::id& proto_id,
                                  const utils::id& id,
                                  const spawn_parameters& prms)
{
    auto obj_to_clone = m_occ->find_obj(proto_id);

    if (!obj_to_clone)
    {
        return nullptr;
    }

    KRG_check(obj_to_clone->get_flags().instance_obj, "Should be always instance");

    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto clone_result = ctor.clone_obj(*obj_to_clone, id);
    if (!clone_result)
    {
        return nullptr;
    }
    auto result = clone_result.value();

    m_occ->reset_loaded_objects();

    auto obj = result->as<root::game_object>();

    obj->update_root();

    if (prms.position)
    {
        obj->set_position(prms.position.value());
    }
    if (prms.rotation)
    {
        obj->set_rotation(prms.rotation.value());
    }
    if (prms.scale)
    {
        obj->set_scale(prms.scale.value());
    }

    glob::glob_state().getr_queues().get_model().dirty_render_components.emplace_back(
        obj->get_root_component());

    m_tickable_objects.emplace_back(obj);

    return result;
}

root::smart_object*
level::spawn_object_impl(const utils::id& proto_id,
                         const utils::id& object_id,
                         const root::smart_object::construct_params& p)
{
    object_constructor ctor(m_occ.get(), object_load_type::instance_obj);
    auto result = ctor.construct_obj(proto_id, object_id, p);
    if (!result)
    {
        return nullptr;
    }

    auto obj = result.value()->as<root::game_object>();
    if (obj)
    {
        glob::glob_state().getr_queues().get_model().dirty_render_components.emplace_back(
            obj->get_root_component());

        m_tickable_objects.emplace_back(obj);
    }

    return result.value();
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
level::unregister_objects()
{
    container::unregister_in_global_cache(
        m_instance_local_cs, *glob::glob_state().get_class_set(), m_id, "instance");
}

void
level::unload()
{
    container::unload();

    m_tickable_objects.clear();
    m_package_ids.clear();

    m_state = level_state::unloaded;
}

}  // namespace core
}  // namespace kryga

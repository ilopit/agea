#include "model/object_constructor.h"

#include "model/components/component.h"
#include "model/game_object.h"
#include "model/mesh_object.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/class_object_cache.h"
#include "model/object_construction_context.h"

#include "serialization/serialization.h"

#include "utils/agea_log.h"

#include "core/fs_locator.h"

#include <fstream>

namespace agea
{
namespace model
{
bool
object_constructor::read_container(const std::string& object_id,
                                   serialization::conteiner& conteiner,
                                   category c)
{
    auto path = glob::resource_locator::get()->resource(c, object_id);

    return serialization::read_container(path, conteiner);
}

bool
object_constructor::object_properties_load(smart_object& obj,
                                           const serialization::conteiner& jc,
                                           object_constructor_context& occ)
{
    auto& properties = obj.reflection()->m_serilalization_properties;

    for (auto& p : properties)
    {
        if (!reflection::property::deserialize(*p, obj, jc, occ))
        {
        }
    }

    return true;
}

bool
object_constructor::object_save(serialization::conteiner&, const std::string&)
{
    return true;
}

smart_object*
object_constructor::class_object_load(const std::string& object_path,
                                      object_constructor_context& occ)
{
    serialization::conteiner conteiner;
    if (!serialization::read_container(object_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    auto type_id = conteiner["type_id"].as<std::string>();
    auto class_id = conteiner["id"].as<std::string>();

    auto tryobj = occ.class_obj_cache->get(class_id);

    AGEA_check(!tryobj, "We should not re-load objects");

    auto eoc = glob::empty_objects_cache::get();
    auto empty = eoc->get(type_id)->META_create_empty_obj();

    object_properties_load(*empty, conteiner, occ);

    occ.class_obj_cache->insert(empty, object_path);
    occ.last_obj = empty;

    return empty.get();
}

smart_object*
object_constructor::object_clone_create(const std::string& object_id,
                                        const std::string& new_object_id,
                                        object_constructor_context& occ)
{
    auto obj = occ.class_obj_cache->get(object_id);

    if (!obj)
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return object_clone_create(*obj, new_object_id, occ);
}

smart_object*
object_constructor::object_clone_create(smart_object& obj,
                                        const std::string& new_object_id,
                                        object_constructor_context& occ)
{
    auto empty = obj.META_create_empty_obj();
    empty->META_set_id(new_object_id);
    occ.temporary_obj_cache.push_back(empty);
    occ.last_obj = empty;

    if (!clone_object_properties(obj, *empty, occ))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    empty->set_class_obj(&obj);

    return empty.get();
}

bool
object_constructor::update_object_properties(smart_object& obj, const serialization::conteiner& jc)
{
    auto& reflection = *obj.reflection();

    object_constructor_context c;
    for (auto k : jc)
    {
        auto key_name = k.first.as<std::string>();
        auto itr = std::find_if(reflection.m_properties.begin(), reflection.m_properties.end(),
                                [&key_name](std::shared_ptr<::agea::reflection::property>& p)
                                { return p->name == key_name; });

        if (itr == reflection.m_properties.end())
        {
            ALOG_WARN("Redundant key - [{0}:{1}] exist", obj.id(), key_name);
            continue;
        }

        auto& p = *itr;
        if (!reflection::property::deserialize_update(*p, (blob_ptr)&obj, jc, c))
        {
            ALOG_ERROR("Property update [{0}:{1}] failed", obj.id(), key_name);
            return false;
        }
    }

    return true;
}

bool
object_constructor::clone_object_properties(smart_object& from,
                                            smart_object& to,
                                            object_constructor_context& occ)
{
    auto& properties = from.reflection()->m_serilalization_properties;

    for (auto& p : properties)
    {
        if (!reflection::property::copy(*p, *p, from, to, occ))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

object_constructor_context&
default_occ()
{
    return glob::level::get()->occ();
}

}  // namespace model
}  // namespace agea

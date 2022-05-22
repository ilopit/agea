#include "model/object_constructor.h"

#include "model/components/component.h"
#include "model/game_object.h"
#include "model/mesh_object.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/class_object_cache.h"
#include "model/object_construction_context.h"

#include "utils/agea_log.h"

#include "core/fs_locator.h"

#include <fstream>
#include <json/json.h>

namespace agea
{
namespace model
{
bool
object_constructor::read_container(const std::string& object_id,
                                   serialization::json_conteiner& conteiner,
                                   category c)
{
    auto path = glob::resource_locator::get()->resource(c, object_id);

    return read_container(path, conteiner);
}

bool
object_constructor::read_container(const std::string& path,
                                   serialization::json_conteiner& conteiner)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    Json::Value json;
    file >> conteiner;

    return true;
}

void
object_constructor::object_properties_load(smart_object& obj,
                                           serialization::json_conteiner& jc,
                                           object_constructor_context& occ)
{
    auto& properties = obj.reflection()->m_serilalization_properties;

    for (auto& p : properties)
    {
        reflection::property::deserialize(*p, obj, jc, occ);
    }
}

bool
object_constructor::object_save(serialization::json_conteiner&, const std::string&)
{
    return true;
}

smart_object*
object_constructor::class_object_load(const std::string& object_path,
                                      object_constructor_context& occ)
{
    serialization::json_conteiner conteiner;
    if (!read_container(object_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    auto type_id = conteiner["type_id"].asString();
    auto class_id = conteiner["id"].asString();

    auto tryobj = occ.class_cache->get(class_id);

    AGEA_check(!tryobj, "We should not re-load objects");

    auto eoc = glob::empty_objects_cache::get();
    auto empty = eoc->get(type_id)->META_create_empty_obj();

    object_properties_load(*empty, conteiner, occ);

    occ.class_cache->insert(empty, object_path);
    occ.last_obj = empty;

    return empty.get();
}

smart_object*
object_constructor::object_clone_create(const std::string& object_id,
                                        const std::string& new_object_id,
                                        object_constructor_context& occ)
{
    auto obj = occ.class_cache->get(object_id);

    if (!obj)
    {
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
    occ.temporary_cache.push_back(empty);
    occ.last_obj = empty;

    clone_object_properties(obj, *empty, occ);

    empty->set_class_obj(&obj);

    return empty.get();
}

bool
object_constructor::update_object_properties(smart_object& obj, serialization::json_conteiner& jc)
{
    auto& reflection = *obj.reflection();

    auto keys = jc.getMemberNames();

    object_constructor_context c;
    for (auto& k : keys)
    {
        auto itr = std::find_if(reflection.m_properties.begin(), reflection.m_properties.end(),
                                [k](std::shared_ptr<::agea::reflection::property>& p)
                                { return p->name == k.c_str(); });

        if (itr == reflection.m_properties.end())
        {
            ALOG_WARN("Redundant key - [{0}:{1}] exist", obj.id(), k.c_str());
            continue;
        }

        auto& p = *itr;
        if (!reflection::property::deserialize_update(*p, (blob_ptr)&obj, jc, c))
        {
            ALOG_ERROR("Property update [{0}:{1}] failed", obj.id(), k.c_str());
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

#include "model/object_constructor.h"

#include "model/components/component.h"
#include "model/game_object.h"
#include "model/mesh_object.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/class_object_cache.h"
#include "model/object_construction_context.h"

#include "model/level.h"

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

    reflection::deserialize_context dc;
    dc.occ = &occ;
    dc.sc = &jc;
    dc.obj = &obj;

    for (auto& p : properties)
    {
        dc.p = p.get();
        if (!p->deserialization_handler(dc))
        {
        }
    }

    return true;
}

bool
object_constructor::object_properties_save(const smart_object& obj, serialization::conteiner& jc)
{
    auto& properties = obj.reflection()->m_serilalization_properties;

    reflection::serialize_context sc;
    sc.sc = &jc;
    sc.obj = &obj;

    for (auto& p : properties)
    {
        sc.p = p.get();
        if (!p->serialization_handler(sc))
        {
        }
    }

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

    AGEA_check(!(occ.class_obj_cache->exists(class_id)), "We should not re-load objects");

    auto eoc = glob::empty_objects_cache::get();
    auto empty = eoc->get(type_id)->META_create_empty_obj();
    empty->META_set_id(class_id);

    object_properties_load(*empty, conteiner, occ);

    occ.class_obj_cache->insert(empty, object_path, occ.m_class_object_order);
    ++occ.m_class_object_order;
    occ.last_obj = empty;

    return empty.get();
}

bool
object_constructor::class_object_save(const smart_object& obj, const std::string& object_path)
{
    serialization::conteiner conteiner;

    conteiner["type_id"] = obj.get_type_id();
    conteiner["id"] = obj.get_id();

    if (!object_properties_save(obj, conteiner))
    {
        return false;
    }

    if (!serialization::write_container(object_path, conteiner))
    {
        return false;
    }

    return true;
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

    empty->META_set_class_obj(&obj);

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
            ALOG_WARN("Redundant key - [{0}:{1}] exist", obj.get_id(), key_name);
            continue;
        }

        auto& p = *itr;
        if (!reflection::property::deserialize_update(*p, (blob_ptr)&obj, jc, c))
        {
            ALOG_ERROR("Property update [{0}:{1}] failed", obj.get_id(), key_name);
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

    reflection::copy_context ctx{nullptr, nullptr, &from, &to, &occ};

    for (auto& p : properties)
    {
        ctx.dst_property = p.get();
        ctx.src_property = p.get();

        if (!p->copy_handler(ctx))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

bool
object_constructor::diff_object_properties(const smart_object& left,
                                           const smart_object& right,
                                           std::vector<reflection::property*>& diff)
{
    if (left.get_type_id() != right.get_type_id())
    {
        return false;
    }

    auto& properties = left.reflection()->m_serilalization_properties;

    reflection::compare_context compare_ctx{nullptr, &left, &right};

    for (auto& p : properties)
    {
        compare_ctx.p = p.get();

        auto same = p->compare_handler(compare_ctx);

        if (!same)
        {
            diff.push_back(p.get());
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

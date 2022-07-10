#include "model/object_constructor.h"

#include "model/components/component.h"
#include "model/game_object.h"
#include "model/mesh_object.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/class_object_cache.h"
#include "model/object_construction_context.h"

#include "model/level.h"

#include "serialization/serialization.h"

#include "utils/agea_log.h"

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
object_constructor::object_load_internal(const utils::path& package_path,
                                         object_constructor_context& occ)
{
    auto full_path = occ.get_full_path(package_path);

    std::string ext, name;
    full_path.parse_file_name_and_ext(name, ext);

    if (name.empty() || ext.empty() || ext != "aobj")
    {
        ALOG_ERROR("Loading object failed, {0} {1}", name, ext);
        return nullptr;
    }
    auto obj_id = utils::id::from(name);

    auto obj = occ.find_class_obj(obj_id);

    if (obj)
    {
        return obj;
    }

    serialization::conteiner conteiner;
    if (!serialization::read_container(full_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    auto class_load = conteiner["class_id"].IsDefined();

    auto id = utils::id::from(conteiner["id"].as<std::string>());

    if (class_load)
    {
        auto class_id = utils::id::from(conteiner["class_id"].as<std::string>());
        auto src_obj = occ.find_class_obj(class_id);

        if (!src_obj)
        {
            auto p = package_path.parent() / (class_id.str() + ".aobj");
            src_obj = object_load_internal(p, occ);
        }

        if (!src_obj)
        {
            return nullptr;
        }

        obj = object_load_partial(*src_obj, conteiner, occ);
    }
    else
    {
        obj = object_load_full(conteiner, occ);
    }

    if (obj->get_id() != utils::id::from(name))
    {
        ALOG_ERROR("File name and id missmatch {0} != {1}", name, obj->get_id().cstr());
    }

    return obj;
}

smart_object*
object_constructor::class_object_load(const utils::path& package_path,
                                      object_constructor_context& occ)
{
    AGEA_check(occ.m_construction_type == obj_construction_type__nav, "Should be only nav");

    occ.m_construction_type = obj_construction_type__class;
    auto obj = object_load_internal(package_path, occ);
    occ.m_construction_type = obj_construction_type__nav;

    return obj;
}

smart_object*
object_constructor::instance_object_load(const utils::path& package_path,
                                         object_constructor_context& occ)
{
    AGEA_check(occ.m_construction_type == obj_construction_type__nav, "Should be only nav");

    occ.m_construction_type = obj_construction_type__instance;
    auto obj = object_load_internal(package_path, occ);
    occ.m_construction_type = obj_construction_type__nav;

    return obj;
}

bool
object_constructor::class_object_save(const smart_object& obj, const utils::path& object_path)
{
    serialization::conteiner conteiner;

    if (!object_save_full(conteiner, obj))
    {
        return false;
    }

    if (!serialization::write_container(object_path, conteiner))
    {
        return false;
    }

    return true;
}

bool
object_constructor::instance_object_save(const smart_object& obj, const utils::path& object_path)
{
    serialization::conteiner conteiner;

    auto has_parent_obj = !obj.is_class_obj();

    if (has_parent_obj)
    {
        if (!object_save_partial(conteiner, obj))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }
    else
    {
        if (!object_save_full(conteiner, obj))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    if (!serialization::write_container(object_path, conteiner))
    {
        return false;
    }

    return true;
}

smart_object*
object_constructor::object_clone_create(const utils::id& object_id,
                                        const utils::id& new_object_id,
                                        object_constructor_context& occ)
{
    auto obj = occ.m_class_local_set.objects->get_item(object_id);

    if (!obj)
    {
        ALOG_INFO("Cache miss {0} try in global!", object_id.str());
        obj = occ.m_class_global_set.objects->get_item(object_id);
    }

    if (!obj)
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return object_clone_create(*obj, new_object_id, occ);
}

smart_object*
object_constructor::object_clone_create(smart_object& obj,
                                        const utils::id& new_object_id,
                                        object_constructor_context& occ)
{
    auto empty = obj.META_create_empty_obj();
    empty->META_set_id(new_object_id);
    empty->META_set_class_obj(&obj);
    occ.add_obj(empty);

    if (!clone_object_properties(obj, *empty, occ))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return empty.get();
}

bool
object_constructor::update_object_properties(smart_object& obj,
                                             const serialization::conteiner& jc,
                                             object_constructor_context& occ)
{
    auto& reflection = *obj.reflection();

    for (auto k : jc)
    {
        auto key_name = k.first.as<std::string>();
        auto itr = std::find_if(reflection.m_properties.begin(), reflection.m_properties.end(),
                                [&key_name](std::shared_ptr<::agea::reflection::property>& p)
                                { return p->name == key_name; });

        if (itr == reflection.m_properties.end())
        {
            ALOG_WARN("Redundant key - [{0}:{1}] exist", obj.get_id().cstr(), key_name);
            continue;
        }

        auto& p = *itr;
        if (!reflection::property::deserialize_update(*p, (blob_ptr)&obj, jc, occ))
        {
            ALOG_ERROR("Property update [{0}:{1}] failed", obj.get_id().str(), key_name);
            return false;
        }
    }

    return true;
}

bool
object_constructor::prototype_object_properties(smart_object& from,
                                                smart_object& to,
                                                const serialization::conteiner& c,
                                                object_constructor_context& occ)
{
    auto& properties = from.reflection()->m_serilalization_properties;

    reflection::property_prototype_context ctx{nullptr, nullptr, &from, &to, &occ, &c};

    for (auto& p : properties)
    {
        ctx.dst_property = p.get();
        ctx.src_property = p.get();

        if (!p->protorype_handler(ctx))
        {
            ALOG_LAZY_ERROR;
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

smart_object*
object_constructor::object_load_full(serialization::conteiner& sc, object_constructor_context& occ)
{
    auto type_id = utils::id::from(sc["type_id"].as<std::string>());
    auto obj_id = utils::id::from(sc["id"].as<std::string>());

    auto eoc_item = glob::empty_objects_cache::get()->get(type_id);

    auto empty = eoc_item->META_create_empty_obj();
    empty->META_set_id(obj_id);
    occ.add_obj(empty);

    if (!object_properties_load(*empty, sc, occ))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return empty.get();
}

bool
object_constructor::object_save_full(serialization::conteiner& sc, const smart_object& obj)
{
    sc["type_id"] = obj.get_type_id().str();
    sc["id"] = obj.get_id().str();

    if (!object_properties_save(obj, sc))
    {
        return false;
    }

    return true;
}

smart_object*
object_constructor::object_load_partial(smart_object& prototype_obj,
                                        serialization::conteiner& sc,
                                        object_constructor_context& occ)
{
    auto empty = prototype_obj.META_create_empty_obj();
    auto obj_id = utils::id::from(sc["id"].as<std::string>());

    empty->META_set_id(obj_id);
    empty->META_set_class_obj(&prototype_obj);
    occ.add_obj(empty);

    if (!prototype_object_properties(prototype_obj, *empty, sc, occ))
    {
        return nullptr;
    }

    return empty.get();
}

bool
object_constructor::object_save_partial(serialization::conteiner& sc, const smart_object& obj)
{
    auto class_obj = obj.get_class_obj();

    if (!class_obj)
    {
        return false;
    }

    sc["class_id"] = class_obj->get_id().str();
    sc["id"] = obj.get_id().str();

    std::vector<reflection::property*> diff;
    model::object_constructor::diff_object_properties(*class_obj, obj, diff);

    reflection::serialize_context ser_ctx{nullptr, &obj, &sc};

    for (auto& p : diff)
    {
        ser_ctx.p = p;

        p->serialization_handler(ser_ctx);
    }

    return true;
}

}  // namespace model
}  // namespace agea

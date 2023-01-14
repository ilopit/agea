#include "model/object_constructor.h"

#include "model/components/component.h"
#include "model/game_object.h"
#include "model/mesh_object.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/caches_map.h"
#include "model/object_construction_context.h"

#include "model/level.h"

#include "serialization/serialization.h"

#include "utils/agea_log.h"

#include <fstream>

namespace agea
{
namespace model
{

namespace
{
void
update_flags(obj_construction_type type, smart_object& obj)
{
    if (type == obj_construction_type::mirror_obj)
    {
        obj.set_state(smart_object_internal_state::mirror);
    }
    else if (type == obj_construction_type::instance_obj)
    {
        obj.set_state(smart_object_internal_state::instance_obj);
    }
    else if (type == obj_construction_type::class_obj)
    {
        obj.set_state(smart_object_internal_state::class_obj);
    }
}

}  // namespace

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
            // return false;
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
            return false;
        }
    }

    return true;
}

smart_object*
object_constructor::object_load_internal(const utils::path& path_in_package,
                                         object_constructor_context& occ)
{
    utils::path full_path;
    if (!occ.make_full_path(path_in_package, full_path))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    serialization::conteiner conteiner;
    if (!serialization::read_container(full_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return object_load_internal(conteiner, occ);
}

smart_object*
object_constructor::object_load_internal(serialization::conteiner& conteiner,
                                         object_constructor_context& occ)
{
    auto use_class_as_prototype = conteiner["class_id"].IsDefined();
    auto has_default_type = conteiner["type_id"].IsDefined();

    auto id = AID(conteiner["id"].as<std::string>());

    smart_object* obj = nullptr;
    if (use_class_as_prototype)
    {
        AGEA_check(!has_default_type, "Should not be here");

        auto class_id = AID(conteiner["class_id"].as<std::string>());
        auto src_obj = occ.find_class_obj(class_id);

        if (!src_obj)
        {
            // Try to load class object and use mapping as hint
            utils::path p;
            if (!occ.make_full_path(class_id, p))
            {
                ALOG_LAZY_ERROR;
                return nullptr;
            }

            // We cannot find class object, let's load it
            auto prev_construction_type = occ.get_construction_type();
            occ.set_construction_type(obj_construction_type::class_obj);
            src_obj = object_load_internal(p, occ);
            occ.set_construction_type(prev_construction_type);
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

    if (obj && (obj->get_id() != id))
    {
        ALOG_LAZY_ERROR;
    }
    return obj;
}

smart_object*
object_constructor::object_load(const utils::id& id, object_constructor_context& occ)
{
    AGEA_check(occ.get_construction_type() != obj_construction_type::nav, "Should be only nav");

    smart_object* obj = occ.get_construction_type() == obj_construction_type::class_obj
                            ? occ.find_class_obj(id)
                            : occ.find_obj(id);
    if (obj)
    {
        ALOG_INFO("Found in cache!");
        return obj;
    }

    if (occ.get_construction_type() == obj_construction_type::mirror_obj)
    {
        ALOG_INFO("Failed to find in mapping");
        return nullptr;
    }

    utils::path full_path;
    if (!occ.make_full_path(id, full_path))
    {
        ALOG_INFO("Failed to find in mapping");
        return nullptr;
    }

    serialization::conteiner c;
    if (!serialization::read_container(full_path, c))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return object_load_internal(c, occ);
}

bool
object_constructor::object_save(const smart_object& obj, const utils::path& object_path)
{
    serialization::conteiner conteiner;

    auto has_parent_obj = obj.has_state(smart_object_internal_state::inhereted);

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
    AGEA_check(occ.get_construction_type() != obj_construction_type::class_obj,
               "Should not be here!");

    auto obj = occ.find_obj(new_object_id);

    if (obj)
    {
        return obj;
    }

    obj = occ.get_class_local_set()->objects->get_item(object_id);

    if (!obj)
    {
        ALOG_INFO("Cache miss {0} try in global!", object_id.str());
        obj = occ.get_class_global_set()->objects->get_item(object_id);
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

    update_flags(occ.get_construction_type(), obj);

    empty->set_state(smart_object_internal_state::inhereted);

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
    auto type_id = AID(sc["type_id"].as<std::string>());
    auto obj_id = AID(sc["id"].as<std::string>());

    auto empty = create_empty_object(type_id, obj_id);

    empty->set_state(smart_object_internal_state::standalone);
    update_flags(occ.get_construction_type(), *empty);

    occ.add_obj(empty);

    if (!object_properties_load(*empty, sc, occ))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    return empty.get();
}

std::shared_ptr<smart_object>
object_constructor::create_empty_object(const utils::id& type_id, const utils::id& obj_id)
{
    auto eoc_item = glob::empty_objects_cache::get()->get(type_id);

    auto empty = eoc_item->META_create_empty_obj();
    empty->META_set_id(obj_id);

    return empty;
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
    auto obj_id = AID(sc["id"].as<std::string>());

    empty->META_set_id(obj_id);
    empty->META_set_class_obj(&prototype_obj);
    occ.add_obj(empty);

    empty->set_state(smart_object_internal_state::inhereted);
    update_flags(occ.get_construction_type(), *empty);

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

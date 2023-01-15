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
update_flags(object_constructor_context::construction_type type, smart_object& obj)
{
    if (type == object_constructor_context::construction_type::mirror_obj)
    {
        obj.set_state(smart_object_internal_state::mirror);
    }
    else if (type == object_constructor_context::construction_type::instance_obj)
    {
        obj.set_state(smart_object_internal_state::instance_obj);
    }
    else if (type == object_constructor_context::construction_type::class_obj)
    {
        obj.set_state(smart_object_internal_state::class_obj);
    }
}

}  // namespace

result_code
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

        auto result = p->deserialization_handler(dc);

        auto has_deserialized =
            result == result_code::ok || (result == result_code::doesnt_exist && p->has_default);

        if (!has_deserialized)
        {
            ALOG_ERROR("Failed to load [{0}:{1}] [{2}]", obj.get_type_id().cstr(), p->name.c_str(),
                       obj.get_id().cstr());
            return result;
        }
    }

    return result_code::ok;
}

result_code
object_constructor::object_properties_save(const smart_object& obj, serialization::conteiner& jc)
{
    auto& properties = obj.reflection()->m_serilalization_properties;

    reflection::serialize_context sc;
    sc.sc = &jc;
    sc.obj = &obj;

    for (auto& p : properties)
    {
        sc.p = p.get();
        auto r = p->serialization_handler(sc);
        if (r != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return r;
        }
    }

    return result_code::ok;
}

result_code
object_constructor::object_load(const utils::path& path_in_package,
                                object_constructor_context& occ,
                                smart_object*& obj)
{
    utils::path full_path;
    if (!occ.make_full_path(path_in_package, full_path))
    {
        ALOG_LAZY_ERROR;
        return result_code::path_not_found;
    }

    serialization::conteiner conteiner;
    if (!serialization::read_container(full_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return result_code::serialization_error;
    }

    return object_load(conteiner, occ, obj);
}

result_code
object_constructor::object_load(serialization::conteiner& conteiner,
                                object_constructor_context& occ,
                                smart_object*& obj)
{
    auto has_prototype_object = conteiner["class_id"].IsDefined();
    auto id = AID(conteiner["id"].as<std::string>());

    if (has_prototype_object)
    {
        AGEA_check(!conteiner["type_id"].IsDefined(), "Should not be here");

        auto class_id = AID(conteiner["class_id"].as<std::string>());
        auto src_obj = occ.find_class_obj(class_id);

        if (!src_obj)
        {
            // Try to load class object and use mapping as hint
            utils::path p;
            if (!occ.make_full_path(class_id, p))
            {
                ALOG_LAZY_ERROR;
                return result_code::path_not_found;
            }

            // We cannot find class object, let's load it
            auto prev_construction_type = occ.get_construction_type();
            occ.set_construction_type(object_constructor_context::construction_type::class_obj);
            auto result = object_load(p, occ, src_obj);
            occ.set_construction_type(prev_construction_type);

            if (result != result_code::ok)
            {
                obj = nullptr;
                ALOG_LAZY_ERROR;
                return result;
            }
        }

        auto result = object_load_partial(*src_obj, conteiner, occ, obj);
        if (result != result_code::ok)
        {
            obj = nullptr;
            ALOG_LAZY_ERROR;
            return result;
        }
    }
    else
    {
        auto result = object_load_full(conteiner, occ, obj);
        if (result != result_code::ok)
        {
            obj = nullptr;
            ALOG_LAZY_ERROR;
            return result;
        }
    }

    if (obj && (obj->get_id() != id))
    {
        ALOG_LAZY_ERROR;
    }

    return result_code::ok;
}

result_code
object_constructor::object_load(const utils::id& id,
                                object_constructor_context& occ,
                                smart_object*& obj)
{
    AGEA_check(
        occ.get_construction_type() == object_constructor_context::construction_type::class_obj ||
            occ.get_construction_type() ==
                object_constructor_context::construction_type::instance_obj,
        "Should be only nav");

    obj = occ.get_construction_type() == object_constructor_context::construction_type::class_obj
              ? occ.find_class_obj(id)
              : occ.find_obj(id);
    if (obj)
    {
        ALOG_TRACE("Found in cache!");
        return result_code::ok;
    }

    utils::path full_path;
    if (!occ.make_full_path(id, full_path))
    {
        ALOG_INFO("Failed to find in mapping");
        return result_code::path_not_found;
    }

    serialization::conteiner c;
    if (!serialization::read_container(full_path, c))
    {
        ALOG_LAZY_ERROR;
        return result_code::serialization_error;
    }

    return object_load(c, occ, obj);
}

result_code
object_constructor::object_save(const smart_object& obj, const utils::path& object_path)
{
    serialization::conteiner conteiner;

    auto has_parent_obj = obj.has_state(smart_object_internal_state::inhereted);

    if (has_parent_obj)
    {
        auto rc = object_save_partial(conteiner, obj);
        if (rc != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
    }
    else
    {
        auto rc = object_save_full(conteiner, obj);
        if (rc != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return rc;
        }
    }

    if (!serialization::write_container(object_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return result_code::serialization_error;
    }

    return result_code::ok;
}

result_code
object_constructor::object_clone_create(const utils::id& object_id,
                                        const utils::id& new_object_id,
                                        object_constructor_context& occ,
                                        smart_object*& obj)
{
    AGEA_check(
        occ.get_construction_type() != object_constructor_context::construction_type::class_obj,
        "Should not be here!");

    obj = occ.find_obj(new_object_id);

    if (obj)
    {
        return result_code::ok;
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
        return result_code::failed;
    }

    return object_clone_create(*obj, new_object_id, occ, obj);
}

result_code
object_constructor::object_clone_create(smart_object& proto_obj,
                                        const utils::id& new_object_id,
                                        object_constructor_context& occ,
                                        smart_object*& obj)
{
    auto empty = create_empty_object(proto_obj.get_type_id(), new_object_id);
    empty->META_set_class_obj(&proto_obj);

    occ.add_obj(empty);

    update_flags(occ.get_construction_type(), *empty);
    empty->set_state(smart_object_internal_state::inhereted);

    auto rc = clone_object_properties(proto_obj, *empty, occ);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    obj = empty.get();

    return result_code::ok;
}

result_code
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
        auto result = reflection::property::deserialize_update(*p, (blob_ptr)&obj, jc, occ);
        if (result != result_code::ok)
        {
            ALOG_ERROR("Property update [{0}:{1}] failed", obj.get_id().str(), key_name);
            return result;
        }
    }

    return result_code::ok;
}

result_code
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

        auto result = p->protorype_handler(ctx);
        if (result != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return result;
        }
    }

    return result_code::ok;
}

result_code
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

        auto result = p->copy_handler(ctx);
        if (result != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return result;
        }
    }

    return result_code::ok;
}

result_code
object_constructor::diff_object_properties(const smart_object& left,
                                           const smart_object& right,
                                           std::vector<reflection::property*>& diff)
{
    if (left.get_type_id() != right.get_type_id())
    {
        return result_code::failed;
    }

    auto& properties = left.reflection()->m_serilalization_properties;

    reflection::compare_context compare_ctx{nullptr, &left, &right};

    for (auto& p : properties)
    {
        compare_ctx.p = p.get();

        auto same = p->compare_handler(compare_ctx) == result_code::ok;

        if (!same)
        {
            diff.push_back(p.get());
        }
    }

    return result_code::ok;
}

object_constructor_context&
default_occ()
{
    return glob::level::get()->occ();
}

result_code
object_constructor::object_load_full(serialization::conteiner& sc,
                                     object_constructor_context& occ,
                                     smart_object*& obj)
{
    AGEA_check(
        occ.get_construction_type() != object_constructor_context::construction_type::mirror_obj,
        "Should not be here!");

    auto type_id = AID(sc["type_id"].as<std::string>());
    auto obj_id = AID(sc["id"].as<std::string>());

    auto empty = create_empty_object(type_id, obj_id);

    empty->set_state(smart_object_internal_state::standalone);
    update_flags(occ.get_construction_type(), *empty);

    occ.add_obj(empty);

    auto result = object_properties_load(*empty, sc, occ);
    if (result != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return result;
    }

    obj = empty.get();
    return result_code::ok;
}

std::shared_ptr<smart_object>
object_constructor::create_empty_object(const utils::id& type_id, const utils::id& obj_id)
{
    auto eoc_item = glob::empty_objects_cache::get()->get(type_id);

    auto empty = eoc_item->META_create_empty_obj();
    empty->META_set_id(obj_id);

    return empty;
}

result_code
object_constructor::object_save_full(serialization::conteiner& sc, const smart_object& obj)
{
    sc["type_id"] = obj.get_type_id().str();
    sc["id"] = obj.get_id().str();

    return object_properties_save(obj, sc);
}

result_code
object_constructor::object_load_partial(smart_object& prototype_obj,
                                        serialization::conteiner& sc,
                                        object_constructor_context& occ,
                                        smart_object*& obj)
{
    AGEA_check(
        occ.get_construction_type() != object_constructor_context::construction_type::mirror_obj,
        "Should not be here!");

    auto obj_id = AID(sc["id"].as<std::string>());
    auto empty = create_empty_object(prototype_obj.get_type_id(), obj_id);

    empty->META_set_class_obj(&prototype_obj);
    empty->set_state(smart_object_internal_state::inhereted);
    update_flags(occ.get_construction_type(), *empty);

    occ.add_obj(empty);

    auto rc = prototype_object_properties(prototype_obj, *empty, sc, occ);

    if (rc == result_code::ok)
    {
        obj = empty.get();
    }

    return rc;
}

result_code
object_constructor::object_save_partial(serialization::conteiner& sc, const smart_object& obj)
{
    AGEA_check(obj.has_state(smart_object_internal_state::inhereted), "");

    auto proto_obj = obj.get_class_obj();

    sc["class_id"] = proto_obj->get_id().str();
    sc["id"] = obj.get_id().str();

    std::vector<reflection::property*> diff;
    auto rc = model::object_constructor::diff_object_properties(*proto_obj, obj, diff);
    if (rc != result_code::ok)
    {
        return rc;
    }

    reflection::serialize_context ser_ctx{nullptr, &obj, &sc};

    for (auto& p : diff)
    {
        ser_ctx.p = p;

        ALOG_INFO("Saving {0}", p->name);
        auto rc = p->serialization_handler(ser_ctx);
        if (rc != result_code::ok)
        {
            return rc;
        }
    }

    return result_code::ok;
}

}  // namespace model
}  // namespace agea

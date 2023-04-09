#include "model/object_constructor.h"

#include "model/components/component.h"
#include "model/game_object.h"
#include "model/components/component.h"
#include "model/package.h"
#include "model/mesh_object.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/caches_map.h"
#include "model/object_load_context.h"

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
update_flags(object_load_type type, smart_object& obj)
{
    if (type == object_load_type::mirror_copy)
    {
        obj.set_flag(smart_object_state_flag::mirror);
    }
    else if (type == object_load_type::instance_obj)
    {
        obj.set_flag(smart_object_state_flag::instance_obj);
    }
    else if (type == object_load_type::class_obj)
    {
        obj.set_flag(smart_object_state_flag::proto_obj);
    }
}

}  // namespace

result_code
object_constructor::object_load(const utils::path& path_in_package,
                                object_load_type type,
                                object_load_context& occ,
                                smart_object*& obj,
                                std::vector<smart_object*>& loaded_obj)
{
    AGEA_check(occ.get_construction_type() == object_load_type::nav, "Should be nav!");

    occ.set_construction_type(type);
    auto rc = object_load_internal(path_in_package, occ, obj);
    occ.set_construction_type(object_load_type::nav);

    occ.reset_loaded_objects(loaded_obj);

    return rc;
}

result_code
object_constructor::object_load(const utils::id& id,
                                object_load_type type,
                                object_load_context& occ,
                                smart_object*& obj,
                                std::vector<smart_object*>& loaded_obj)
{
    AGEA_check(occ.get_construction_type() == object_load_type::nav, "Should be nav!");

    occ.set_construction_type(type);
    auto rc = object_load_internal(id, occ, obj);
    occ.set_construction_type(object_load_type::nav);

    occ.reset_loaded_objects(loaded_obj);

    return rc;
}

result_code
object_constructor::mirror_object(const utils::id& class_object_id,
                                  object_load_context& occ,
                                  smart_object*& obj,
                                  std::vector<smart_object*>& loaded_obj)
{
    AGEA_check(occ.get_construction_type() == object_load_type::nav, "Should be nav!");

    occ.set_construction_type(model::object_load_type::mirror_copy);
    auto rc = object_clone_create_internal(class_object_id, class_object_id, occ, obj);
    occ.set_construction_type(model::object_load_type::nav);

    occ.reset_loaded_objects(loaded_obj);

    return rc;
}

result_code
object_constructor::object_clone(smart_object& src,
                                 const utils::id& new_object_id,
                                 object_load_context& occ,
                                 smart_object*& obj,
                                 std::vector<smart_object*>& loaded_obj)
{
    AGEA_check(occ.get_construction_type() == object_load_type::nav, "Should be nav!");

    occ.set_construction_type(model::object_load_type::instance_obj);
    auto rc = object_clone_create_internal(src, new_object_id, occ, obj);
    occ.set_construction_type(model::object_load_type::nav);

    occ.reset_loaded_objects(loaded_obj);

    return rc;
}

smart_object*
object_constructor::alloc_empty_object(const utils::id& type_id,
                                       const utils::id& id,
                                       uint32_t extra_flags,
                                       object_load_context& olc)
{
    auto eoc_item = glob::empty_objects_cache::get()->get_item(type_id);

    AGEA_check(eoc_item, "Should exist!");

    auto empty = eoc_item->META_create_empty_obj();
    empty->META_set_id(id);
    empty->set_flag(extra_flags);
    empty->set_package(olc.get_package());
    empty->set_level(olc.get_level());

    auto ptr = empty.get();
    update_flags(olc.get_construction_type(), *empty);

    olc.add_obj(std::move(empty), olc.get_global_load_mode());
    olc.push_object_loaded(ptr);

    return ptr;
}

smart_object*
object_constructor::object_construct(const utils::id& type_id,
                                     const utils::id& id,
                                     const model::smart_object::construct_params& params,
                                     object_load_context& olc)
{
    auto package = olc.get_package();
    auto level = olc.get_level();

    smart_object* obj = nullptr;

    if (package)
    {
        auto prev_ct = olc.get_construction_type();

        AGEA_check(prev_ct != object_load_type::instance_obj, "ct missmatch!");

        olc.set_construction_type(object_load_type::class_obj);
        obj = object_constructor::alloc_empty_object(type_id, id, 0, olc);
        if (!obj->META_construct(params))
        {
            return nullptr;
        }

        if (!obj->post_construct())
        {
            return nullptr;
        }

        olc.set_construction_type(object_load_type::mirror_copy);
        obj = object_constructor::alloc_empty_object(type_id, id, 0, olc);
        if (!obj->META_construct(params))
        {
            return nullptr;
        }
        if (!obj->post_construct())
        {
            return nullptr;
        }

        olc.set_construction_type(prev_ct);
    }
    else if (level)
    {
        AGEA_check(olc.get_construction_type() == object_load_type::mirror_copy, "ct missmatch!");

        olc.set_construction_type(object_load_type::mirror_copy);
        obj = object_constructor::alloc_empty_object(type_id, id, 0, olc);
        if (!obj->META_construct(params))
        {
            return nullptr;
        }
        if (!obj->post_construct())
        {
            return nullptr;
        }
    }

    return obj;
}

result_code
object_constructor::register_package_type_impl(std::shared_ptr<smart_object> empty,
                                               object_load_context& olc)
{
    AGEA_check(olc.get_construction_type() == object_load_type::nav, "Should be nav!");
    AGEA_check(olc.get_package(), "Should exist");
    AGEA_check(!olc.get_level(), "Should exist");

    glob::empty_objects_cache::get()->add_item(*empty);

    empty->set_flag(smart_object_state_flag::proto_obj | smart_object_state_flag::empty_obj);

    olc.set_construction_type(object_load_type::class_obj);
    olc.add_obj(empty, false);
    olc.set_construction_type(object_load_type::mirror_copy);

    alloc_empty_object(empty->get_id(), empty->get_id(), smart_object_state_flag::empty_obj, olc);
    olc.set_construction_type(object_load_type::nav);

    return result_code::ok;
}

result_code
object_constructor::object_properties_load(smart_object& obj,
                                           const serialization::conteiner& jc,
                                           object_load_context& occ)
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

    auto empty_obj = glob::empty_objects_cache::getr().get_item(obj.reflection()->type_name);

    reflection::serialize_context sc;
    reflection::compare_context cc;
    sc.sc = &jc;
    sc.obj = &obj;

    cc.dst_obj = &obj;
    cc.src_obj = empty_obj;

    for (auto& p : properties)
    {
        sc.p = p.get();

        if (p->has_default)
        {
            cc.p = p.get();
            auto rc = p->compare_handler(cc);
            if (rc == result_code::ok)
            {
                continue;
            }
        }

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
object_constructor::object_load_internal(const utils::path& path_in_package,
                                         object_load_context& occ,
                                         smart_object*& obj)
{
    AGEA_check(occ.get_construction_type() != object_load_type::nav, "Should be nav!");

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

    return object_load_internal(conteiner, occ, obj);
}

result_code
object_constructor::object_load_internal(serialization::conteiner& conteiner,
                                         object_load_context& occ,
                                         smart_object*& obj)
{
    AGEA_check(occ.get_construction_type() != object_load_type::nav, "Should be nav!");

    auto has_prototype_object = conteiner["class_id"].IsDefined();
    auto id = AID(conteiner["id"].as<std::string>());

    if (has_prototype_object)
    {
        AGEA_check(!conteiner["type_id"].IsDefined(), "Should not be here");

        auto class_id = AID(conteiner["class_id"].as<std::string>());
        auto src_obj = occ.get_construction_type() != object_load_type::class_obj
                           ? occ.find_obj(class_id)
                           : occ.find_class_obj(class_id);

        if (!src_obj)
        {
            obj = nullptr;
            ALOG_LAZY_ERROR;
            return result_code::proto_doesnt_exist;
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

    obj->set_state(smart_object_state::loaded);

    if (obj && (obj->get_id() != id))
    {
        ALOG_LAZY_ERROR;
    }

    return result_code::ok;
}

result_code
object_constructor::object_load_internal(const utils::id& id,
                                         object_load_context& occ,
                                         smart_object*& obj)
{
    AGEA_check(occ.get_construction_type() != object_load_type::nav, "Should be nav!");

    obj = occ.get_construction_type() == object_load_type::class_obj ? occ.find_class_obj(id)
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

    return object_load_internal(c, occ, obj);
}

result_code
object_constructor::object_save(const smart_object& obj, const utils::path& object_path)
{
    AGEA_check(!obj.has_flag(smart_object_state_flag::mirror), "Mirro object should not be saved");

    serialization::conteiner conteiner;

    auto has_parent_obj = obj.has_flag(smart_object_state_flag::inhereted);

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
object_constructor::object_clone_create_internal(const utils::id& object_id,
                                                 const utils::id& new_object_id,
                                                 object_load_context& occ,
                                                 smart_object*& obj)
{
    AGEA_check(object_load_type::class_obj != occ.get_construction_type(), "Should not happen");

    obj = occ.find_obj(new_object_id);

    if (obj)
    {
        return result_code::ok;
    }

    obj = occ.get_construction_type() == object_load_type::mirror_copy
              ? occ.find_class_obj(object_id)
              : occ.find_obj(object_id);

    if (!obj)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return object_clone_create_internal(*obj, new_object_id, occ, obj);
}

result_code
object_constructor::object_clone_create_internal(smart_object& proto_obj,
                                                 const utils::id& new_object_id,
                                                 object_load_context& occ,
                                                 smart_object*& obj)
{
    AGEA_check(object_load_type::class_obj != occ.get_construction_type(), "Should not happen");

    obj = alloc_empty_object(proto_obj.get_type_id(), new_object_id,
                             smart_object_state_flag::inhereted, occ);

    obj->META_set_class_obj(&proto_obj);

    auto rc = clone_object_properties(proto_obj, *obj, occ);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    obj->set_state(smart_object_state::loaded);

    return result_code::ok;
}

result_code
object_constructor::update_object_properties(smart_object& obj,
                                             const serialization::conteiner& jc,
                                             object_load_context& occ)
{
    auto& reflection = *obj.reflection();

    for (auto k : jc)
    {
        auto key_name = k.first.as<std::string>();
        auto itr = std::find_if(reflection.m_properties.begin(), reflection.m_properties.end(),
                                [&key_name](const std::shared_ptr<::agea::reflection::property>& p)
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
                                                object_load_context& occ)
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
                                            object_load_context& occ)
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

result_code
object_constructor::object_load_full(serialization::conteiner& sc,
                                     object_load_context& occ,
                                     smart_object*& obj)
{
    AGEA_check(occ.get_construction_type() != object_load_type::mirror_copy, "Should not be here!");

    auto type_id = AID(sc["type_id"].as<std::string>());
    auto obj_id = AID(sc["id"].as<std::string>());

    obj = alloc_empty_object(type_id, obj_id, smart_object_state_flag::standalone, occ);

    return object_properties_load(*obj, sc, occ);
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
                                        object_load_context& occ,
                                        smart_object*& obj)
{
    AGEA_check(occ.get_construction_type() != object_load_type::mirror_copy, "Should not be here!");

    auto obj_id = AID(sc["id"].as<std::string>());

    obj = alloc_empty_object(prototype_obj.get_type_id(), obj_id,
                             smart_object_state_flag::inhereted, occ);

    obj->META_set_class_obj(&prototype_obj);

    auto rc = prototype_object_properties(prototype_obj, *obj, sc, occ);

    if (rc != result_code::ok)
    {
        return rc;
    }

    obj->set_state(smart_object_state::loaded);

    return rc;
}

result_code
object_constructor::object_save_partial(serialization::conteiner& sc, const smart_object& obj)
{
    AGEA_check(obj.has_flag(smart_object_state_flag::inhereted), "");

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

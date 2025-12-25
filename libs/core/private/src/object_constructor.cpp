#include "core/object_constructor.h"

#include "core/package.h"
#include "core/caches/caches_map.h"
#include "core/object_load_context.h"
#include "core/level.h"
#include "global_state/global_state.h"
#include "core/reflection/reflection_type.h"

#include <packages/root/model/components/component.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/components/component.h>

#include <serialization/serialization.h>

#include <utils/agea_log.h>

#include <fstream>

namespace agea
{
namespace core
{

std::expected<root::smart_object*, result_code>
object_constructor::object_load(const utils::id& id,
                                object_load_type type,
                                object_load_context& occ,
                                std::vector<root::smart_object*>& loaded_obj)
{
    auto old_objects = occ.reset_loaded_objects();

    occ.push_construction_type(type);
    auto result = object_load_internal(id, occ);
    occ.pop_construction_type();

    occ.reset_loaded_objects(old_objects, loaded_obj);

    for (auto o : loaded_obj)
    {
        o->post_load();
    }

    return result;
}

std::expected<root::smart_object*, result_code>
object_constructor::object_clone(root::smart_object& src,
                                 object_load_type type,
                                 const utils::id& new_object_id,
                                 object_load_context& occ,
                                 std::vector<root::smart_object*>& loaded_obj)
{
    auto old_objects = occ.reset_loaded_objects();

    occ.push_construction_type(type);
    auto result = object_clone_create_internal(src, new_object_id, occ);
    occ.pop_construction_type();

    occ.reset_loaded_objects(old_objects, loaded_obj);

    for (auto o : loaded_obj)
    {
        o->post_load();
    }

    return result;
}

std::expected<root::smart_object*, result_code>
object_constructor::object_instantiate(root::smart_object& src,
                                       const utils::id& new_object_id,
                                       object_load_context& occ,
                                       std::vector<root::smart_object*>& loaded_obj)
{
    auto old_objects = occ.reset_loaded_objects();

    occ.push_construction_type(core::object_load_type::instance_obj);
    auto result = object_instanciate_internal(src, new_object_id, occ);
    occ.pop_construction_type();

    occ.reset_loaded_objects(old_objects, loaded_obj);

    for (auto o : loaded_obj)
    {
        o->post_load();
    }
    return result;
}

std::expected<root::smart_object*, result_code>
object_constructor::alloc_empty_object(const utils::id& type_id,
                                       const utils::id& id,
                                       root::smart_object_flags flags,
                                       root::smart_object* parent_object,
                                       object_load_context& olc)
{
    auto rt = glob::glob_state().get_rm()->get_type(type_id);

    if (!rt)
    {
        return std::unexpected(result_code::id_not_found);
    }

    reflection::type_alloc_context alloc_ctx{&id};
    auto empty = rt->alloc(alloc_ctx);
    empty->set_package(olc.get_package());
    empty->set_level(olc.get_level());
    empty->META_set_class_obj(parent_object);
    empty->get_flags() = flags;

    auto ptr = empty.get();
    olc.add_obj(std::move(empty));
    olc.push_object_loaded(ptr);

    return ptr;
}

std::expected<root::smart_object*, result_code>
object_constructor::object_construct(const utils::id& type_id,
                                     const utils::id& id,
                                     const root::smart_object::construct_params& params,
                                     object_load_context& olc)
{
    auto package = olc.get_package();
    auto level = olc.get_level();

    root::smart_object* obj = nullptr;

    if (package)
    {
        olc.push_construction_type(object_load_type::class_obj);
        auto alloc_result =
            object_constructor::alloc_empty_object(type_id, id, ks_class_constructed, nullptr, olc);
        if (!alloc_result)
        {
            olc.pop_construction_type();
            return alloc_result;
        }
        obj = alloc_result.value();

        if (!obj->META_construct(params))
        {
            olc.pop_construction_type();
            return std::unexpected(result_code::failed);
        }

        if (!obj->post_construct())
        {
            olc.pop_construction_type();
            return std::unexpected(result_code::failed);
        }

        olc.pop_construction_type();
    }
    else if (level)
    {
        olc.push_construction_type(object_load_type::instance_obj);
        auto alloc_result = object_constructor::alloc_empty_object(
            type_id, id, ks_instance_constructed, nullptr, olc);
        if (!alloc_result)
        {
            olc.pop_construction_type();
            return alloc_result;
        }
        obj = alloc_result.value();

        if (!obj->META_construct(params))
        {
            olc.pop_construction_type();
            return alloc_result;
        }
        if (!obj->post_construct())
        {
            olc.pop_construction_type();
            return alloc_result;
        }

        olc.pop_construction_type();
    }

    olc.reset_loaded_objects();

    return obj;
}

std::expected<root::smart_object*, result_code>
object_constructor::create_default_class_obj_impl(std::shared_ptr<root::smart_object> empty,
                                                  object_load_context& olc)
{
    AGEA_check(olc.get_construction_type() == object_load_type::class_obj, "Should be class!");
    AGEA_check(olc.get_package(), "Should exist");
    AGEA_check(!olc.get_level(), "Should exist");

    empty->get_flags() = ks_class_default;

    olc.push_construction_type(object_load_type::class_obj);
    empty->set_package(olc.get_package());
    olc.add_obj(empty);
    olc.pop_construction_type();

    auto rt = empty->get_reflection();

    auto p = rt->cparams_alloc();
    if (!empty->META_construct(*p))
    {
        return std::unexpected(result_code::failed);
    }

    if (!empty->post_construct())
    {
        return std::unexpected(result_code::failed);
    }

    return empty.get();
}

result_code
object_constructor::destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc)
{
    auto obj = olc.find_proto_obj(id);

    AGEA_check(obj, "Should exists");

    auto go = obj->as<root::game_object>();

    if (go)
    {
        auto& comps = go->get_subcomponents();

        for (auto ritr = comps.rbegin(); ritr != comps.rend(); ++ritr)
        {
            auto& obj = **ritr;
            olc.remove_obj(obj);
        }
    }

    olc.remove_obj(*obj);

    return result_code::ok;
}

result_code
object_constructor::object_properties_save(const root::smart_object& obj,
                                           serialization::conteiner& jc)
{
    auto& properties = obj.get_reflection()->m_serilalization_properties;

    auto empty_obj =
        glob::glob_state().get_class_objects_cache()->get_item(obj.get_reflection()->type_name);

    reflection::save_context sc;
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

        auto r = p->save_handler(sc);
        if (r != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return r;
        }
    }

    return result_code::ok;
}

result_code
object_constructor::object_save(const root::smart_object& obj, const utils::path& object_path)
{
    serialization::conteiner conteiner;

    auto has_parent_obj = obj.get_flags().derived_obj;

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

std::expected<root::smart_object*, result_code>
object_constructor::object_clone_create_internal(const utils::id& object_id,
                                                 const utils::id& new_object_id,
                                                 object_load_context& occ)
{
    AGEA_check(object_load_type::class_obj != occ.get_construction_type(), "Should not happen");

    auto obj = occ.find_obj(new_object_id);

    if (obj)
    {
        return obj;
    }

    obj = occ.get_construction_type() == object_load_type::class_obj ? occ.find_proto_obj(object_id)
                                                                     : occ.find_obj(object_id);

    if (!obj)
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(result_code::failed);
    }

    return object_clone_create_internal(*obj, new_object_id, occ);
}

std::expected<root::smart_object*, result_code>
object_constructor::object_clone_create_internal(root::smart_object& proto_obj,
                                                 const utils::id& new_object_id,
                                                 object_load_context& occ)
{
    AGEA_check(&proto_obj, "Should exist!");

    root::smart_object_flags flags{.derived_obj = true};
    if (occ.get_construction_type() == object_load_type::instance_obj)
        flags.instance_obj = true;

    auto alloc_result =
        alloc_empty_object(proto_obj.get_type_id(), new_object_id, flags, &proto_obj, occ);
    if (!alloc_result)
    {
        return std::unexpected(alloc_result.error());
    }
    auto obj = alloc_result.value();

    auto rc = clone_object_properties(proto_obj, *obj, occ);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    return obj;
}

std::expected<root::smart_object*, result_code>
object_constructor::object_instanciate_internal(root::smart_object& proto_obj,
                                                const utils::id& new_object_id,
                                                object_load_context& occ)
{
    AGEA_check(&proto_obj, "Should exist!");

    AGEA_check(object_load_type::class_obj != occ.get_construction_type(), "Should not happen");

    auto alloc_result = alloc_empty_object(proto_obj.get_type_id(), new_object_id,
                                           ks_instance_derived, &proto_obj, occ);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = instantiate_object_properties(proto_obj, *obj, occ);
    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    return obj;
}


result_code
object_constructor::load_derive_object_properties(root::smart_object& from,
                                                  root::smart_object& to,
                                                  const serialization::conteiner& c,
                                                  object_load_context& occ)
{
    auto& properties = from.get_reflection()->m_serilalization_properties;

    reflection::property_load_context ctx{nullptr, nullptr, &from, &to, &occ, &c};

    for (auto& p : properties)
    {
        ctx.dst_property = p.get();
        ctx.src_property = p.get();

        auto result = p->load_handler(ctx);
        if (result != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return result;
        }
    }

    return result_code::ok;
}

result_code
object_constructor::clone_object_properties(root::smart_object& from,
                                            root::smart_object& to,
                                            object_load_context& occ)
{
    auto& properties = from.get_reflection()->m_serilalization_properties;

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
object_constructor::instantiate_object_properties(root::smart_object& from,
                                                  root::smart_object& to,
                                                  object_load_context& occ)
{
    auto& properties = from.get_reflection()->m_serilalization_properties;

    reflection::instantiate_context ictx{nullptr, nullptr, &from, &to, &occ};
    reflection::copy_context cctx{nullptr, nullptr, &from, &to, &occ};

    for (auto& p : properties)
    {
        if (p->instantiate_handler)
        {
            ictx.dst_property = p.get();
            ictx.src_property = p.get();

            auto result = p->instantiate_handler(ictx);
            if (result != result_code::ok)
            {
                ALOG_LAZY_ERROR;
                return result;
            }
        }
        else
        {
            cctx.dst_property = p.get();
            cctx.src_property = p.get();

            auto result = p->copy_handler(cctx);
            if (result != result_code::ok)
            {
                ALOG_LAZY_ERROR;
                return result;
            }
        }
    }

    return result_code::ok;
}

result_code
object_constructor::diff_object_properties(const root::smart_object& left,
                                           const root::smart_object& right,
                                           std::vector<reflection::property*>& diff)
{
    if (&left == &right)
    {
        return result_code::ok;
    }

    if (left.get_type_id() != right.get_type_id())
    {
        return result_code::failed;
    }

    auto& properties = left.get_reflection()->m_serilalization_properties;

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
object_constructor::object_save_full(serialization::conteiner& sc, const root::smart_object& obj)
{
    sc["type_id"] = obj.get_type_id().str();
    sc["id"] = obj.get_id().str();

    return object_properties_save(obj, sc);
}

std::expected<root::smart_object*, result_code>
object_constructor::object_load_derive(root::smart_object& prototype_obj,
                                       serialization::conteiner& sc,
                                       object_load_context& occ)
{
    auto obj_id = AID(sc["id"].as<std::string>());

    auto flags = occ.get_construction_type() == object_load_type::instance_obj ? ks_instance_derived
                                                                               : ks_class_derived;

    auto alloc_result =
        alloc_empty_object(prototype_obj.get_type_id(), obj_id, flags, &prototype_obj, occ);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = load_derive_object_properties(prototype_obj, *obj, sc, occ);

    if (rc != result_code::ok)
    {
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    return obj;
}

result_code
object_constructor::object_save_partial(serialization::conteiner& sc, const root::smart_object& obj)
{
    auto proto_obj = obj.get_class_obj();

    sc["class_id"] = proto_obj->get_id().str();
    sc["id"] = obj.get_id().str();

    std::vector<reflection::property*> diff;
    auto rc = core::object_constructor::diff_object_properties(*proto_obj, obj, diff);
    if (rc != result_code::ok)
    {
        return rc;
    }

    reflection::save_context ser_ctx{nullptr, &obj, &sc};

    for (auto& p : diff)
    {
        ser_ctx.p = p;

        auto rc = p->save_handler(ser_ctx);
        if (rc != result_code::ok)
        {
            return rc;
        }
    }

    return result_code::ok;
}

std::expected<root::smart_object*, result_code>
object_constructor::object_load_internal(const utils::id& id, object_load_context& occ)
{
    AGEA_check(occ.get_construction_type() != object_load_type::nav, "Should be nav!");

    auto obj = occ.get_construction_type() == object_load_type::class_obj ? occ.find_proto_obj(id)
                                                                          : occ.find_obj(id);
    if (obj)
    {
        ALOG_TRACE("Found in cache!");
        return obj;
    }

    if (occ.get_construction_type() == object_load_type::instance_obj)
    {
        // We don't want to work with proto objects directly.We have to make sure that we clone
        // proto as instance

        if (auto proto = preload_proto(id, occ))
        {
            return object_instanciate_internal(*proto.value(), proto.value()->get_id(), occ);
        }
    }

    utils::path full_path;
    if (!occ.make_full_path(id, full_path))
    {
        ALOG_ERROR("Failed to find in mapping");
        return std::unexpected(result_code::path_not_found);
    }

    serialization::conteiner c;
    if (!serialization::read_container(full_path, c))
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(result_code::serialization_error);
    }

    return object_load_internal(c, occ);
}

std::expected<root::smart_object*, result_code>
object_constructor::preload_proto(const utils::id& id, object_load_context& occ)
{
    if (auto proto_obj = occ.find_proto_obj(id))
    {
        return proto_obj;
    }

    if (auto rt = glob::glob_state().get_rm()->get_type(id))
    {
        ALOG_INFO("Creating default object {}", id.str());
        reflection::type_alloc_context alloc_ctx{&id};
        return object_constructor::create_default_class_obj_impl(rt->alloc(alloc_ctx), occ);
    }

    if (!occ.get_package())
    {
        return std::unexpected(result_code::failed);
    }

    std::vector<root::smart_object*> objcts;
    return object_load(id, object_load_type::class_obj, occ, objcts);
}

std::expected<root::smart_object*, result_code>
object_constructor::create_default_default_class_proto(const utils::id& id,
                                                       object_load_context& olc)
{
    olc.push_construction_type(object_load_type::class_obj);
    if (auto rt = glob::glob_state().get_rm()->get_type(id))
    {
        ALOG_INFO("Creating default object {}", id.str());
        reflection::type_alloc_context alloc_ctx{&id};
        auto result = object_constructor::create_default_class_obj_impl(rt->alloc(alloc_ctx), olc);
        olc.pop_construction_type();
        return result;
    }
    olc.pop_construction_type();

    return std::unexpected(result_code::id_not_found);
}

std::expected<root::smart_object*, result_code>
object_constructor::object_load_internal(serialization::conteiner& conteiner,
                                         object_load_context& occ)
{
    AGEA_check(occ.get_construction_type() != object_load_type::nav, "Should be nav!");

    auto id = AID(conteiner["id"].as<std::string>());

    root::smart_object* obj = nullptr;
    auto proto_id = conteiner["class_id"].IsDefined() ? AID(conteiner["class_id"].as<std::string>())
                                                      : AID(conteiner["type_id"].as<std::string>());

    auto src_result = preload_proto(proto_id, occ);
    if (!src_result)
    {
        ALOG_ERROR("Proto object [{}] doesn't exist", proto_id.cstr());
        return src_result;
    }

    auto result = object_load_derive(*src_result.value(), conteiner, occ);
    if (!result)
    {
        ALOG_LAZY_ERROR;
        return result;
    }
    obj = result.value();

    if (obj && (obj->get_id() != id))
    {
        ALOG_LAZY_ERROR;
    }

    return obj;
}

}  // namespace core
}  // namespace agea

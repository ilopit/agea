#include "core/object_constructor.h"

#include "core/package.h"
#include "core/caches/caches_map.h"
#include "core/object_load_context.h"
#include "core/level.h"
#include "global_state/global_state.h"
#include "core/reflection/reflection_type.h"

#include <packages/root/model/components/component.h>
#include <packages/root/model/game_object.h>

#include <serialization/serialization.h>

#include <utils/kryga_log.h>

#include <fstream>

namespace kryga
{
namespace core
{

std::expected<root::smart_object*, result_code>
object_constructor::create_default_class_obj_impl(reflection::reflection_type* rt)
{
    auto id = rt->type_name;
    reflection::type_context__alloc alloc_ctx{&id};

    auto empty = rt->alloc(alloc_ctx);

    empty->get_flags() = ks_class_default;

    empty->set_package(m_olc->get_package());
    m_olc->add_obj(empty);

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

std::expected<root::smart_object*, result_code>
object_constructor::preload_proto(const utils::id& id)
{
    if (auto proto_obj = m_olc->find_proto_obj(id))
    {
        return proto_obj;
    }

    // Default class object
    if (auto rt = glob::glob_state().get_rm()->get_type(id))
    {
        return object_constructor::create_default_class_obj_impl(rt);
    }

    if (!m_olc->get_package())
    {
        return std::unexpected(result_code::failed);
    }

    std::vector<root::smart_object*> objects;
    return load_package_obj(id);
}

std::expected<kryga::root::smart_object*, kryga::result_code>
object_constructor::object_load_derive(root::smart_object& prototype_obj,
                                       serialization::container& sc)
{
    auto obj_id = AID(sc["id"].as<std::string>());

    auto flags = m_mode == object_load_type::instance_obj ? ks_instance_derived : ks_class_derived;

    auto alloc_result =
        alloc_empty_object(prototype_obj.get_type_id(), obj_id, flags, &prototype_obj);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = load_derive_object_properties(prototype_obj, *obj, sc);

    if (rc != result_code::ok)
    {
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    if (!obj->post_load())
    {
        ALOG_ERROR("post_load failed for [{}]", obj->get_id().cstr());
        return std::unexpected(result_code::failed);
    }

    return obj;
}

inline std::expected<root::smart_object*, result_code>
object_constructor::alloc_empty_object(const utils::id& type_id,
                                       const utils::id& id,
                                       root::smart_object_flags flags,
                                       root::smart_object* parent_object)
{
    auto rt = glob::glob_state().get_rm()->get_type(type_id);

    if (!rt)
    {
        return std::unexpected(result_code::id_not_found);
    }

    reflection::type_context__alloc alloc_ctx{&id};
    auto empty = rt->alloc(alloc_ctx);
    empty->set_package(m_olc->get_package());
    empty->set_level(m_olc->get_level());
    empty->META_set_class_obj(parent_object);
    empty->get_flags() = flags;

    auto ptr = empty.get();
    m_olc->add_obj(std::move(empty));
    m_olc->push_object_loaded(ptr);

    return ptr;
}

std::expected<root::smart_object*, result_code>
object_constructor::object_load_internal(serialization::container& container)
{
    auto id = AID(container["id"].as<std::string>());

    auto proto_id = container["class_id"].IsDefined() ? AID(container["class_id"].as<std::string>())
                                                      : AID(container["type_id"].as<std::string>());

    auto src_result = preload_proto(proto_id);
    if (!src_result)
    {
        ALOG_ERROR("Proto object [{}] doesn't exist", proto_id.cstr());
        return src_result;
    }

    auto result = object_load_derive(*src_result.value(), container);
    if (!result)
    {
        ALOG_ERROR("Failed to load object [{}] with proto [{}]", id.cstr(), proto_id.cstr());
        return result;
    }
    auto obj = result.value();

    if (obj && (obj->get_id() != id))
    {
        ALOG_ERROR("Object id mismatch: expected [{}], got [{}]", id.cstr(), obj->get_id().cstr());
    }

    return obj;
}

result_code
object_constructor::load_derive_object_properties(root::smart_object& from,
                                                  root::smart_object& to,
                                                  const serialization::container& c)
{
    auto& properties = from.get_reflection()->m_serialization_properties;

    reflection::property_context__load ctx{nullptr, nullptr, &from, &to, this, &c};

    for (auto& p : properties)
    {
        ctx.dst_property = p.get();
        ctx.src_property = p.get();

        auto result = p->load_handler(ctx);
        if (result != result_code::ok)
        {
            ALOG_ERROR("Failed to load property [{}] on object [{}]", p->name, to.get_id().cstr());
            return result;
        }
    }

    return result_code::ok;
}

std::expected<root::smart_object*, result_code>
object_constructor::load_package_obj(const utils::id& id)
{
    KRG_check(m_olc, "");
    KRG_check(m_olc->get_package(), "");

    if (auto obj = m_olc->find_proto_obj(id))
    {
        ALOG_TRACE("Found in cache!");
        return obj;
    }

    if (auto rt = glob::glob_state().get_rm()->get_type(id))
    {
        return object_constructor::create_default_class_obj_impl(rt);
    }

    utils::path full_path;
    if (!m_olc->make_full_path(id, full_path))
    {
        ALOG_ERROR("Failed to find [{}] in mapping", id.cstr());
        return std::unexpected(result_code::path_not_found);
    }

    serialization::container c;
    if (!serialization::read_container(full_path, c))
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(result_code::serialization_error);
    }

    return object_load_internal(c);
}

std::expected<root::smart_object*, result_code>
object_constructor::load_level_obj(const utils::id& id)
{
    KRG_check(m_olc, "");
    KRG_check(m_olc->get_level(), "");

    if (auto obj = m_olc->find_obj(id))
    {
        ALOG_TRACE("Found in cache!");
        return obj;
    }

    // Proto already loaded from package — instantiate
    if (auto proto = m_olc->find_proto_obj(id))
    {
        return instantiate_obj(*proto, id);
    }

    // Load from file
    utils::path full_path;
    if (!m_olc->make_full_path(id, full_path))
    {
        ALOG_ERROR("Failed to find [{}] in mapping", id.cstr());
        return std::unexpected(result_code::path_not_found);
    }

    serialization::container c;
    if (!serialization::read_container(full_path, c))
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(result_code::serialization_error);
    }

    // Load derive with instance context
    auto container_id = AID(c["id"].as<std::string>());
    auto proto_id = c["class_id"].IsDefined() ? AID(c["class_id"].as<std::string>())
                                              : AID(c["type_id"].as<std::string>());

    auto proto = m_olc->find_proto_obj(proto_id);
    if (!proto)
    {
        ALOG_ERROR("Proto object [{}] doesn't exist for level obj [{}]", proto_id.cstr(),
                   id.cstr());
        return std::unexpected(result_code::id_not_found);
    }

    object_constructor inst_ctor(m_olc, object_load_type::instance_obj);

    auto alloc_result = inst_ctor.alloc_empty_object(proto->get_type_id(), container_id,
                                                     ks_instance_derived, proto);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = inst_ctor.load_derive_object_properties(*proto, *obj, c);
    if (rc != result_code::ok)
    {
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    if (!obj->post_load())
    {
        ALOG_ERROR("post_load failed for level obj [{}]", obj->get_id().cstr());
        return std::unexpected(result_code::failed);
    }

    return obj;
}

std::expected<root::smart_object*, result_code>
object_constructor::instantiate_obj(root::smart_object& proto, const utils::id& new_id)
{
    if (auto obj = m_olc->find_obj(new_id))
    {
        return obj;
    }

    object_constructor inst_ctor(m_olc, object_load_type::instance_obj);

    auto alloc_result =
        inst_ctor.alloc_empty_object(proto.get_type_id(), new_id, ks_instance_derived, &proto);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = inst_ctor.instantiate_object_properties(proto, *obj);
    if (rc != result_code::ok)
    {
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    if (!obj->post_load())
    {
        ALOG_ERROR("post_load failed for [{}]", obj->get_id().cstr());
        return std::unexpected(result_code::failed);
    }

    return obj;
}

std::expected<root::smart_object*, result_code>
object_constructor::load_obj(const utils::id& id)
{
    if (m_mode == object_load_type::class_obj)
    {
        auto obj = m_olc->find_proto_obj(id);
        if (obj)
        {
            return obj;
        }

        if (auto rt = glob::glob_state().get_rm()->get_type(id))
        {
            return create_default_class_obj_impl(rt);
        }
    }
    else
    {
        auto obj = m_olc->find_obj(id);
        if (obj)
        {
            return obj;
        }

        if (auto proto = m_olc->find_proto_obj(id))
        {
            return instantiate_obj(*proto, id);
        }

        // For instance mode: preload as class first, then instantiate
        object_constructor class_ctor(m_olc, object_load_type::class_obj);
        auto proto_result = class_ctor.preload_proto(id);
        if (proto_result)
        {
            return instantiate_obj(*proto_result.value(), proto_result.value()->get_id());
        }
        // If preload failed (e.g. no package), fall through to file loading
    }

    utils::path full_path;
    if (!m_olc->make_full_path(id, full_path))
    {
        ALOG_ERROR("Failed to find [{}] in mapping", id.cstr());
        return std::unexpected(result_code::path_not_found);
    }

    serialization::container c;
    if (!serialization::read_container(full_path, c))
    {
        ALOG_LAZY_ERROR;
        return std::unexpected(result_code::serialization_error);
    }

    return object_load_internal(c);
}

std::expected<root::smart_object*, result_code>
object_constructor::load_obj(serialization::container& c)
{
    return object_load_internal(c);
}

std::expected<root::smart_object*, result_code>
object_constructor::clone_obj(root::smart_object& src, const utils::id& new_id)
{
    auto existing = m_mode == object_load_type::instance_obj ? m_olc->find_obj(new_id)
                                                             : m_olc->find_proto_obj(new_id);
    if (existing)
    {
        return existing;
    }

    root::smart_object_flags flags{.derived_obj = true};
    if (m_mode == object_load_type::instance_obj)
    {
        flags.instance_obj = true;
    }

    auto alloc_result = alloc_empty_object(src.get_type_id(), new_id, flags, &src);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = clone_object_properties(src, *obj);
    if (rc != result_code::ok)
    {
        return std::unexpected(rc);
    }

    obj->set_state(root::smart_object_state::loaded);

    if (!obj->post_load())
    {
        ALOG_ERROR("post_load failed for [{}]", obj->get_id().cstr());
        return std::unexpected(result_code::failed);
    }

    return obj;
}

std::expected<root::smart_object*, result_code>
object_constructor::construct_obj(const utils::id& type_id,
                                  const utils::id& id,
                                  const root::smart_object::construct_params& params)
{
    auto flags =
        m_mode == object_load_type::instance_obj ? ks_instance_constructed : ks_class_constructed;

    auto alloc_result = alloc_empty_object(type_id, id, flags, nullptr);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    if (!obj->META_construct(params))
    {
        return std::unexpected(result_code::failed);
    }

    if (!obj->post_construct())
    {
        return std::unexpected(result_code::failed);
    }

    m_olc->reset_loaded_objects();

    return obj;
}

result_code
object_constructor::clone_object_properties(root::smart_object& from, root::smart_object& to)
{
    auto& properties = from.get_reflection()->m_serialization_properties;

    reflection::property_context__copy ctx{nullptr, nullptr, &from, &to, this};

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
object_constructor::instantiate_object_properties(root::smart_object& from, root::smart_object& to)
{
    auto& properties = from.get_reflection()->m_serialization_properties;

    reflection::property_context__instantiate ictx{nullptr, nullptr, &from, &to, this};
    reflection::property_context__copy cctx{nullptr, nullptr, &from, &to, this};

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

    auto& properties = left.get_reflection()->m_serialization_properties;

    reflection::property_context__compare compare_ctx{nullptr, &left, &right};

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
object_constructor::object_save(const root::smart_object& obj, const utils::path& object_path)
{
    serialization::container container;

    if (auto rc = object_save_internal(container, obj); rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    if (!serialization::write_container(object_path, container))
    {
        ALOG_LAZY_ERROR;
        return result_code::serialization_error;
    }

    return result_code::ok;
}

result_code
object_constructor::object_properties_save(const root::smart_object& obj,
                                           serialization::container& jc)
{
    auto& properties = obj.get_reflection()->m_serialization_properties;

    auto empty_obj =
        glob::glob_state().get_class_objects_cache()->get_item(obj.get_reflection()->type_name);

    reflection::property_context__save sc;
    reflection::property_context__compare cc;
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
object_constructor::destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc)
{
    auto obj = olc.find_proto_obj(id);

    KRG_check(obj, "Should exists");

    auto go = obj->as<root::game_object>();

    if (go)
    {
        auto& comps = go->get_subcomponents();

        for (auto ritr = comps.rbegin(); ritr != comps.rend(); ++ritr)
        {
            auto& comp_obj = **ritr;
            olc.remove_obj(comp_obj);
        }
    }

    olc.remove_obj(*obj);

    return result_code::ok;
}

result_code
object_constructor::object_save_full(serialization::container& sc, const root::smart_object& obj)
{
    sc["type_id"] = obj.get_type_id().str();
    sc["id"] = obj.get_id().str();

    return object_properties_save(obj, sc);
}

result_code
object_constructor::object_save_internal(serialization::container& sc,
                                         const root::smart_object& obj)
{
    auto proto_obj = obj.get_class_obj();
    auto cid = proto_obj->get_id().str();
    sc["class_id"] = cid;

    auto id = obj.get_id().str();
    sc["id"] = id;

    std::vector<reflection::property*> diff;
    auto rc = object_constructor::diff_object_properties(*proto_obj, obj, diff);
    if (rc != result_code::ok)
    {
        return rc;
    }

    reflection::property_context__save ser_ctx{nullptr, &obj, &sc};

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

}  // namespace core
}  // namespace kryga

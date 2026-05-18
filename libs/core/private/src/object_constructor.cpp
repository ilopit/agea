#include "core/object_constructor.h"

#include "core/object_load_context.h"
#include "core/id_generator.h"
#include "core/model_system.h"
#include "core/package.h"
#include "core/level.h"
#include "global_state/global_state.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/reflection/property_utils.h>

#include <serialization/serialization.h>
#include <vfs/vfs.h>

#include "utils/kryga_log.h"
#include "utils/check.h"

namespace kryga
{
namespace core
{

std::expected<root::smart_object*, result_code>
object_constructor::load_obj(const utils::id& id)
{
    if (auto obj = m_olc->find_obj(id))
    {
        return obj;
    }

    if (auto rt = glob::glob_state().getr_model().reflection.get_type(id))
    {
        return create_default_class_obj_impl(rt);
    }

    vfs::rid rid;
    if (!m_olc->resolve(id, rid))
    {
        ALOG_ERROR("Failed to resolve [{}] in VFS", id.cstr());
        return std::unexpected(result_code::path_not_found);
    }

    serialization::container c;
    if (!serialization::read_container(rid, c))
    {
        ALOG_ERROR("Failed to read container for [{}]", id.cstr());
        return std::unexpected(result_code::serialization_error);
    }

    return object_load_internal(c);
}

std::expected<root::smart_object*, result_code>
object_constructor::load_sub_object(serialization::container& c)
{
    return object_load_internal(c);
}

std::expected<root::smart_object*, result_code>
object_constructor::instantiate_obj(root::smart_object& proto, const utils::id& new_id)
{
    if (proto.get_flags().instance_obj)
    {
        ALOG_ERROR("Cannot instantiate from an instance object [{}]", proto.get_id().cstr());
        return std::unexpected(result_code::failed);
    }

    if (auto obj = m_olc->find_obj(new_id))
    {
        if (obj->get_flags().instance_obj)
        {
            return obj;
        }
    }

    auto alloc_result =
        alloc_empty_object(proto.get_type_id(), new_id, ks_instance_derived, &proto);
    if (!alloc_result)
    {
        return alloc_result;
    }
    auto obj = alloc_result.value();

    auto rc = instantiate_object_properties(proto, *obj);
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
object_constructor::clone_obj(root::smart_object& src, const utils::id& new_id)
{
    if (auto existing = m_olc->find_obj(new_id))
    {
        return existing;
    }

    auto flags = m_mode == object_load_type::instance_obj ? ks_instance_derived : ks_class_derived;

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
                                  const root::smart_object::construct_params& params,
                                  bool is_proto)
{
    auto proto_result = preload_proto(type_id);
    if (!proto_result)
    {
        return proto_result;
    }

    if (is_proto)
    {
        KRG_check(!proto_result.value()->get_flags().instance_obj,
                  "Proto object must be constructed from proto objects");
    }

    auto flags = is_proto ? ks_class_derived : ks_instance_derived;
    auto alloc_result = alloc_empty_object(type_id, id, flags, proto_result.value());
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

std::expected<root::smart_object*, result_code>
object_constructor::construct_obj(const utils::id& type_id,
                                  name_of name,
                                  const root::smart_object::construct_params& params,
                                  bool is_proto)
{
    auto& id_gen = glob::glob_state().getr_model().id_gen;
    return construct_obj(type_id, id_gen.generate(name.pattern), params, is_proto);
}

result_code
object_constructor::save_obj(const root::smart_object& obj)
{
    serialization::container sc;

    auto* class_obj = obj.get_class_obj();
    KRG_check(class_obj, "Object must have a class_obj to save against");

    sc["proto_id"] = class_obj->get_id().str();
    sc["id"] = obj.get_id().str();

    std::vector<reflection::property*> diff;
    auto rc = diff_object_properties(*class_obj, obj, diff);
    if (rc != result_code::ok)
    {
        return rc;
    }

    reflection::property_context__save ser_ctx{nullptr, &obj, &sc};

    for (auto& p : diff)
    {
        ser_ctx.p = p;
        auto r = p->save_handler(ser_ctx);
        if (r != result_code::ok)
        {
            return r;
        }
    }

    auto vfs_root = m_olc->get_vfs_root();
    vfs::rid rid;
    std::string relative_path;

    if (m_olc->resolve(obj.get_id(), rid))
    {
        relative_path = std::string(rid.relative());
        if (relative_path.starts_with(vfs_root.relative()))
        {
            relative_path = relative_path.substr(vfs_root.relative().size() + 1);
        }
    }
    else
    {
        relative_path = "class/" + obj.get_id().str() + ".aobj";
        rid = vfs_root / relative_path;
    }

    if (!serialization::write_container(rid, sc))
    {
        ALOG_ERROR("Failed to write object [{}]", obj.get_id().cstr());
        return result_code::serialization_error;
    }

    auto& vfs = glob::glob_state().getr_vfs();
    vfs.register_object(vfs_root, obj.get_id().str(), relative_path);

    return result_code::ok;
}

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
    if (!empty->META_default_construct(*p))
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
object_constructor::object_load_internal(serialization::container& c)
{
    auto id = AID(c["id"].as<std::string>());
    auto proto_id = AID(c["proto_id"].as<std::string>());

    auto src_result = preload_proto(proto_id);
    if (!src_result)
    {
        ALOG_ERROR("Proto object [{}] doesn't exist", proto_id.cstr());
        return src_result;
    }

    auto result = object_load_derive(*src_result.value(), c);
    if (!result)
    {
        ALOG_ERROR("Failed to load object [{}] with proto [{}]", id.cstr(), proto_id.cstr());
        return result;
    }

    return result;
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
object_constructor::preload_proto(const utils::id& id)
{
    if (auto proto_obj = m_olc->find_obj(id))
    {
        return proto_obj;
    }

    if (auto rt = glob::glob_state().getr_model().reflection.get_type(id))
    {
        return create_default_class_obj_impl(rt);
    }

    return std::unexpected(result_code::path_not_found);
}

std::expected<root::smart_object*, result_code>
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

std::expected<root::smart_object*, result_code>
object_constructor::alloc_empty_object(const utils::id& type_id,
                                       const utils::id& id,
                                       root::smart_object_flags flags,
                                       root::smart_object* parent_object)
{
    if (m_olc->find_obj(id))
    {
        ALOG_ERROR("Object with id [{}] already exists", id.cstr());
        return std::unexpected(result_code::failed);
    }

    auto rt = glob::glob_state().getr_model().reflection.get_type(type_id);

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

result_code
object_constructor::clone_object_properties(root::smart_object& from, root::smart_object& to)
{
    auto& properties = from.get_reflection()->m_serialization_properties;

    reflection::property_context__instantiate ictx{nullptr, nullptr, &from, &to, this};
    reflection::property_context__copy cctx{nullptr, nullptr, &from, &to, this};

    for (auto& p : properties)
    {
        result_code result = result_code::ok;

        switch (p->inst_mode)
        {
        case reflection::instantiate_mode::share:
        {
            auto src = p->get_blob(from);
            auto dst = p->get_blob(to);
            if (p->type.is_ptr)
            {
                std::memcpy(dst, src, sizeof(void*));
            }
            else
            {
                cctx.dst_property = p.get();
                cctx.src_property = p.get();
                result = p->copy_handler(cctx);
            }
            break;
        }
        case reflection::instantiate_mode::instantiate:
        {
            if (p->type.is_collection)
            {
                cctx.dst_property = p.get();
                cctx.src_property = p.get();
                result = p->copy_handler(cctx);
            }
            else if (p->rtype && p->rtype->arch == architype::smart_object)
            {
                auto& src_ptr = reflection::utils::as_type<root::smart_object*>(p->get_blob(from));
                auto& dst_ptr = reflection::utils::as_type<root::smart_object*>(p->get_blob(to));
                if (src_ptr)
                {
                    auto& id_gen = glob::glob_state().getr_model().id_gen;
                    auto r = clone_obj(*src_ptr, id_gen.generate(src_ptr->get_id()));
                    if (!r)
                    {
                        result = r.error();
                    }
                    else
                    {
                        dst_ptr = r.value();
                    }
                }
            }
            else
            {
                cctx.dst_property = p.get();
                cctx.src_property = p.get();
                result = p->copy_handler(cctx);
            }
            break;
        }
        }

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
        result_code result = result_code::ok;

        switch (p->inst_mode)
        {
        case reflection::instantiate_mode::share:
        {
            auto src = p->get_blob(from);
            auto dst = p->get_blob(to);
            if (p->type.is_ptr)
            {
                std::memcpy(dst, src, sizeof(void*));
            }
            else
            {
                cctx.dst_property = p.get();
                cctx.src_property = p.get();
                result = p->copy_handler(cctx);
            }
            break;
        }
        case reflection::instantiate_mode::instantiate:
        {
            ictx.dst_property = p.get();
            ictx.src_property = p.get();
            result = p->instantiate_handler(ictx);
            break;
        }
        }

        if (result != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return result;
        }
    }

    return result_code::ok;
}

}  // namespace core
}  // namespace kryga

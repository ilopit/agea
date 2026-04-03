#include "core/reflection/property.h"

#include "core/caches/caches_map.h"
#include "core/reflection/reflection_type.h"
#include "core/reflection/reflection_type_utils.h"

#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/mesh.h>

#include <utils/kryga_log.h>

#include <serialization/serialization.h>

#include <cinttypes>
#include <array>

namespace kryga
{
namespace reflection
{

namespace
{

inline uint8_t*
reduce_ptr(uint8_t* ptr, bool is_ptr)
{
    return is_ptr ? *(uint8_t**)(ptr) : ptr;
}
}  // namespace

result_code
property::default_compare(property_context__compare& context)
{
    if (context.p->type.is_collection)
    {
        return compare_collection(context);
    }
    else
    {
        return compare_item(context);
    }
}

result_code
property::default_save(property_context__save& ctx)
{
    if (ctx.p->type.is_collection)
    {
        return serialize_collection(*ctx.p, *ctx.obj, *ctx.sc);
    }
    else
    {
        return serialize_item(*ctx.p, *ctx.obj, *ctx.sc);
    }
}

result_code
property::default_copy(property_context__copy& cxt)
{
    KRG_check(cxt.src_property->rtype->copy, "Should be valid!");
    KRG_check(cxt.dst_property->rtype->copy, "Should never happen!");

    KRG_check(cxt.src_property == cxt.dst_property, "Should be SAME properties!");
    KRG_check(cxt.src_obj != cxt.dst_obj, "Should not be SAME objects!");

    KRG_check(!cxt.src_property->type.is_collection, "Not supported!");

    auto from = ::kryga::reflection::reduce_ptr(cxt.src_property->get_blob(*cxt.src_obj),
                                                cxt.src_property->type.is_ptr);
    auto to = ::kryga::reflection::reduce_ptr(cxt.dst_property->get_blob(*cxt.dst_obj),
                                              cxt.dst_property->type.is_ptr);

    type_context__copy type_ctx{nullptr, from, nullptr, to, cxt.ctor};
    return cxt.dst_property->rtype->copy(type_ctx);
}

result_code
property::default_instantiate(property_context__instantiate& cxt)
{
    KRG_check(cxt.src_property == cxt.dst_property, "Should be SAME properties!");
    KRG_check(cxt.src_obj != cxt.dst_obj, "Should not be SAME objects!");
    KRG_check(!cxt.src_property->type.is_collection, "Not supported!");

    KRG_check(cxt.src_property->rtype->instantiate || cxt.src_property->rtype->copy,
              "Should be valid!");
    KRG_check(cxt.dst_property->rtype->instantiate || cxt.dst_property->rtype->copy,
              "Should never happen!");

    auto from = ::kryga::reflection::reduce_ptr(cxt.src_property->get_blob(*cxt.src_obj),
                                                cxt.src_property->type.is_ptr);
    auto to = ::kryga::reflection::reduce_ptr(cxt.dst_property->get_blob(*cxt.dst_obj),
                                              cxt.dst_property->type.is_ptr);

    type_context__copy type_ctx{nullptr, from, nullptr, to, cxt.ctor};

    if (cxt.dst_property->rtype->instantiate)
    {
        return cxt.dst_property->rtype->instantiate(type_ctx);
    }

    return cxt.dst_property->rtype->copy(type_ctx);
}

result_code
property::default_load(property_context__load& ctx)
{
    if (!ctx.src_property->rtype->copy)
    {
        return result_code::failed;
    }

    KRG_check(ctx.src_property == ctx.dst_property, "Should be SAME properties!");
    KRG_check(ctx.src_obj != ctx.dst_obj, "Should not be SAME objects!");

    if (ctx.src_property->type.is_collection)
    {
        KRG_not_implemented;
    }
    else
    {
        auto& container = *ctx.sc;
        if (container[ctx.dst_property->name].IsDefined())
        {
            return load_item(*ctx.src_property, *ctx.dst_obj, *ctx.sc, ctx.ctor);
        }
        else
        {
            auto from = ::kryga::reflection::reduce_ptr(ctx.src_property->get_blob(*ctx.src_obj),
                                                        ctx.src_property->type.is_ptr);
            auto to = ::kryga::reflection::reduce_ptr(ctx.dst_property->get_blob(*ctx.dst_obj),
                                                      ctx.dst_property->type.is_ptr);

            KRG_check(ctx.dst_property->rtype->copy, "Should never happen!");
            type_context__copy type_ctx{nullptr, from, nullptr, to, ctx.ctor};
            if (auto rc = ctx.dst_property->rtype->copy(type_ctx); rc != result_code::ok)
            {
                return rc;
            }
        }
    }
    return result_code::ok;
}

kryga::result_code
property::default_to_string(property_context__to_string& ctx)
{
    auto from =
        ::kryga::reflection::reduce_ptr(ctx.prop->get_blob(*ctx.obj), ctx.prop->type.is_ptr);

    type_context__to_string type_ctx{nullptr, from, &ctx.result};
    return ctx.prop->rtype->to_string(type_ctx);
}

kryga::blob_ptr
property::get_blob(root::smart_object& obj)
{
    return obj.as_blob() + offset;
}

result_code
property::deserialize_collection(reflection::property& p,
                                 root::smart_object& obj,
                                 const serialization::container& jc,
                                 core::object_constructor* ctor)
{
    auto ptr = (blob_ptr)&obj;
    auto items = jc[p.name];
    auto items_size = items.size();
    auto& r = utils::as_type<std::vector<void*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }

    KRG_check(p.rtype->load, "Should never happen!");

    for (size_t i = 0; i < items_size; ++i)
    {
        auto item = items[i];
        auto idx = item["order_idx"].as<std::uint32_t>();

        auto* field_ptr = &r[idx];
        type_context__load type_ctx{nullptr, (blob_ptr)field_ptr, &item, ctor};
        if (auto rc = p.rtype->load(type_ctx); rc != result_code::ok)
        {
            return rc;
        }
    }

    return result_code::ok;
}

result_code
property::serialize_collection(const reflection::property&,
                               const root::smart_object&,
                               serialization::container&)
{
    return result_code::ok;
}

result_code
property::load_item(reflection::property& p,
                    root::smart_object& obj,
                    const serialization::container& jc,
                    core::object_constructor* ctor)
{
    if (!jc[p.name])
    {
        ALOG_WARN("Unable to deserialize property [{0}][{1}:{2}] ",
                  obj.get_id().cstr(),
                  obj.get_type_id().str(),
                  p.name);
        return result_code::doesnt_exist;
    }

    auto ptr = obj.as_blob();

    ptr = ::kryga::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    KRG_check(p.rtype->load, "Should never happen!");

    auto sub_jc = jc[p.name];
    type_context__load type_ctx{nullptr, ptr, &sub_jc, ctor};
    return p.rtype->load(type_ctx);
}

result_code
property::serialize_item(const reflection::property& p,
                         const root::smart_object& obj,
                         serialization::container& sc)
{
    auto ptr = (blob_ptr)&obj;

    ptr = ::kryga::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    serialization::container c;

    KRG_check(p.rtype->save, "Should never happen!");
    type_context__save type_ctx{&obj, ptr, &c};
    if (auto rc = p.rtype->save(type_ctx); rc != result_code::ok)
    {
        return rc;
    }

    sc[p.name] = c;

    return result_code::ok;
}

result_code
property::compare_collection(property_context__compare&)
{
    KRG_not_implemented;

    return result_code::failed;
}

result_code
property::compare_item(property_context__compare& context)
{
    auto src_ptr = ::kryga::reflection::reduce_ptr(context.src_obj->as_blob() + context.p->offset,
                                                   context.p->type.is_ptr);
    auto dst_ptr = ::kryga::reflection::reduce_ptr(context.dst_obj->as_blob() + context.p->offset,
                                                   context.p->type.is_ptr);

    type_context__compare type_ctx{src_ptr, dst_ptr};
    return context.p->rtype->compare(type_ctx);
}

}  // namespace reflection
}  // namespace kryga

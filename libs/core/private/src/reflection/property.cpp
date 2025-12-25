#include "core/reflection/property.h"

#include "core/caches/caches_map.h"
#include "core/reflection/reflection_type.h"
#include "core/reflection/reflection_type_utils.h"

#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/mesh.h>

#include <utils/agea_log.h>

#include <serialization/serialization.h>

#include <inttypes.h>
#include <array>

namespace agea
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
property::default_compare(compare_context& context)
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
property::default_save(save_context& ctx)
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
property::default_copy(copy_context& cxt)
{
    AGEA_check(cxt.src_property->rtype->copy, "Should be valid!");
    AGEA_check(cxt.dst_property->rtype->copy, "Should never happens!");

    AGEA_check(cxt.src_property == cxt.dst_property, "Should be SAME properties!");
    AGEA_check(cxt.src_obj != cxt.dst_obj, "Should not be SAME objects!");

    AGEA_check(!cxt.src_property->type.is_collection, "Not supported!");

    auto from = ::agea::reflection::reduce_ptr(cxt.src_property->get_blob(*cxt.src_obj),
                                               cxt.src_property->type.is_ptr);
    auto to = ::agea::reflection::reduce_ptr(cxt.dst_property->get_blob(*cxt.dst_obj),
                                             cxt.dst_property->type.is_ptr);

    type_copy_context type_ctx{cxt.src_obj, cxt.dst_obj, from, to, cxt.occ};
    return cxt.dst_property->rtype->copy(type_ctx);
}

result_code
property::default_instantiate(instantiate_context& cxt)
{
    AGEA_check(cxt.src_property == cxt.dst_property, "Should be SAME properties!");
    AGEA_check(cxt.src_obj != cxt.dst_obj, "Should not be SAME objects!");
    AGEA_check(!cxt.src_property->type.is_collection, "Not supported!");

    AGEA_check(cxt.src_property->rtype->instantiate || cxt.src_property->rtype->copy,
               "Should be valid!");
    AGEA_check(cxt.dst_property->rtype->instantiate || cxt.dst_property->rtype->copy,
               "Should never happens!");

    auto from = ::agea::reflection::reduce_ptr(cxt.src_property->get_blob(*cxt.src_obj),
                                               cxt.src_property->type.is_ptr);
    auto to = ::agea::reflection::reduce_ptr(cxt.dst_property->get_blob(*cxt.dst_obj),
                                             cxt.dst_property->type.is_ptr);

    type_copy_context type_ctx{cxt.src_obj, cxt.dst_obj, from, to, cxt.occ};

    if (cxt.dst_property->rtype->instantiate)
    {
        return cxt.dst_property->rtype->instantiate(type_ctx);
    }

    return cxt.dst_property->rtype->copy(type_ctx);
}

result_code
property::default_load(property_load_context& ctx)
{
    if (!ctx.src_property->rtype->copy)
    {
        return result_code::failed;
    }

    AGEA_check(ctx.src_property == ctx.dst_property, "Should be SAME properties!");
    AGEA_check(ctx.src_obj != ctx.dst_obj, "Should not be SAME objects!");

    if (ctx.src_property->type.is_collection)
    {
        AGEA_not_implemented;
    }
    else
    {
        auto& conteiner = *ctx.sc;
        if (conteiner[ctx.dst_property->name].IsDefined())
        {
            return load_item(*ctx.src_property, *ctx.dst_obj, *ctx.sc, *ctx.occ);
        }
        else
        {
            auto from = ::agea::reflection::reduce_ptr(ctx.src_property->get_blob(*ctx.src_obj),
                                                       ctx.src_property->type.is_ptr);
            auto to = ::agea::reflection::reduce_ptr(ctx.dst_property->get_blob(*ctx.dst_obj),
                                                     ctx.dst_property->type.is_ptr);

            AGEA_check(ctx.dst_property->rtype->copy, "Should never happens!");
            type_copy_context type_ctx{ctx.src_obj, ctx.dst_obj, from, to, ctx.occ};
            ctx.dst_property->rtype->copy(type_ctx);
        }
    }
    return result_code::ok;
}

agea::result_code
property::default_to_string(property_to_string_context& ctx)
{
    auto from = ::agea::reflection::reduce_ptr(ctx.prop->get_blob(*ctx.obj), ctx.prop->type.is_ptr);

    type_ui_context type_ctx{from, &ctx.result};
    return ctx.prop->rtype->to_string(type_ctx);
}

agea::blob_ptr
property::get_blob(root::smart_object& obj)
{
    return obj.as_blob() + offset;
}

result_code
property::deserialize_collection(reflection::property& p,
                                 root::smart_object& obj,
                                 const serialization::conteiner& jc,
                                 core::object_load_context& occ)
{
    auto ptr = (blob_ptr)&obj;
    auto items = jc[p.name];
    auto items_size = items.size();
    auto& r = utils::as_type<std::vector<void*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }

    AGEA_check(p.rtype->load, "Should never happens!");

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];
        auto idx = item["order_idx"].as<std::uint32_t>();

        auto* filed_ptr = &r[idx];
        type_load_context type_ctx{&obj, (blob_ptr)filed_ptr, &item, &occ};
        p.rtype->load(type_ctx);
    }

    return result_code::ok;
}

result_code
property::serialize_collection(const reflection::property&,
                               const root::smart_object&,
                               serialization::conteiner&)
{
    return result_code::ok;
}

result_code
property::load_item(reflection::property& p,
                    root::smart_object& obj,
                    const serialization::conteiner& jc,
                    core::object_load_context& occ)
{
    if (!jc[p.name])
    {
        ALOG_WARN("Unable to deserialize property [{0}][{1}:{2}] ", obj.get_id().cstr(),
                  obj.get_type_id().str(), p.name);
        return result_code::doesnt_exist;
    }

    auto ptr = obj.as_blob();

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    AGEA_check(p.rtype->load, "Should never happens!");

    auto sub_jc = jc[p.name];
    type_load_context type_ctx{&obj, ptr, &sub_jc, &occ};
    return p.rtype->load(type_ctx);
}

result_code
property::serialize_item(const reflection::property& p,
                         const root::smart_object& obj,
                         serialization::conteiner& sc)
{
    auto ptr = (blob_ptr)&obj;

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    serialization::conteiner c;

    AGEA_check(p.rtype->save, "Should never happens!");
    type_save_context type_ctx{&obj, ptr, &c};
    p.rtype->save(type_ctx);

    sc[p.name] = c;

    return result_code::ok;
}

result_code
property::compare_collection(compare_context&)
{
    AGEA_not_implemented;

    return result_code::failed;
}

result_code
property::compare_item(compare_context& context)
{
    auto src_ptr = ::agea::reflection::reduce_ptr(context.src_obj->as_blob() + context.p->offset,
                                                  context.p->type.is_ptr);
    auto dst_ptr = ::agea::reflection::reduce_ptr(context.dst_obj->as_blob() + context.p->offset,
                                                  context.p->type.is_ptr);

    type_compare_context type_ctx{src_ptr, dst_ptr};
    return context.p->rtype->compare(type_ctx);
}

}  // namespace reflection
}  // namespace agea

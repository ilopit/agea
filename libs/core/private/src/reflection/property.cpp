#include "core/reflection/property.h"

#include "core/caches/materials_cache.h"
#include "core/caches/meshes_cache.h"
#include "core/caches/caches_map.h"
#include "core/reflection/reflection_type.h"

#include <packages/root/assets/material.h>
#include <packages/root/assets/mesh.h>

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
property::default_deserialize(deserialize_context& ctx)
{
    if (ctx.p->type.is_collection)
    {
        return deserialize_collection(*ctx.p, *ctx.obj, *ctx.sc, *ctx.occ);
    }
    else
    {
        return deserialize_item(*ctx.p, *ctx.obj, *ctx.sc, *ctx.occ);
    }
}

result_code
property::default_serialize(serialize_context& ctx)
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

    AGEA_check(cxt.src_property == cxt.dst_property, "Should be SAME properties!");
    AGEA_check(cxt.src_obj != cxt.dst_obj, "Should not be SAME objects!");

    AGEA_check(!cxt.src_property->type.is_collection, "Not supported!");

    auto from = ::agea::reflection::reduce_ptr(cxt.src_property->get_blob(*cxt.src_obj),
                                               cxt.src_property->type.is_ptr);
    auto to = ::agea::reflection::reduce_ptr(cxt.dst_property->get_blob(*cxt.dst_obj),
                                             cxt.dst_property->type.is_ptr);

    AGEA_check(cxt.dst_property->rtype->copy, "Should never happens!");
    return cxt.dst_property->rtype->copy(*cxt.src_obj, *cxt.dst_obj, from, to, *cxt.occ);
}

result_code
property::default_prototype(property_prototype_context& ctx)
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
            return deserialize_item(*ctx.src_property, *ctx.dst_obj, *ctx.sc, *ctx.occ);
        }
        else
        {
            auto from = ::agea::reflection::reduce_ptr(ctx.src_property->get_blob(*ctx.src_obj),
                                                       ctx.src_property->type.is_ptr);
            auto to = ::agea::reflection::reduce_ptr(ctx.dst_property->get_blob(*ctx.dst_obj),
                                                     ctx.dst_property->type.is_ptr);

            AGEA_check(ctx.dst_property->rtype->copy, "Should never happens!");
            ctx.dst_property->rtype->copy(*ctx.src_obj, *ctx.dst_obj, from, to, *ctx.occ);
        }
    }
    return result_code::ok;
}

agea::blob_ptr
property::get_blob(root::smart_object& obj)
{
    return obj.as_blob() + offset;
}

result_code
property::deserialize_update(reflection::property& p,
                             blob_ptr ptr,
                             const serialization::conteiner& jc,
                             core::object_load_context& occ)
{
    if (p.type.is_collection)
    {
        return deserialize_update_collection(p, ptr, jc, occ);
    }
    else
    {
        return deserialize_update_item(p, ptr, jc, occ);
    }
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

    AGEA_check(p.rtype->deserialization, "Should never happens!");

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];
        auto idx = item["order_idx"].as<std::uint32_t>();

        auto* filed_ptr = &r[idx];
        p.rtype->deserialization(obj, (blob_ptr)filed_ptr, item, occ);
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
property::deserialize_item(reflection::property& p,
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

    AGEA_check(p.rtype->deserialization, "Should never happens!");

    return p.rtype->deserialization(obj, ptr, jc[p.name], occ);
}

result_code
property::serialize_item(const reflection::property& p,
                         const root::smart_object& obj,
                         serialization::conteiner& sc)
{
    auto ptr = (blob_ptr)&obj;

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    serialization::conteiner c;

    AGEA_check(p.rtype->serialization, "Should never happens!");
    p.rtype->serialization(obj, ptr, c);

    sc[p.name] = c;

    return result_code::ok;
}

result_code
property::deserialize_update_collection(reflection::property& p,
                                        blob_ptr ptr,
                                        const serialization::conteiner& jc,
                                        core::object_load_context& occ)
{
    auto items = jc[p.name];
    auto items_size = items.size();
    auto& r = utils::as_type<std::vector<root::component*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }
    AGEA_check(p.rtype->deserialization_with_proto, "Should never happens!");
    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];

        auto* filed_ptr = &r[i];
        p.rtype->deserialization_with_proto((blob_ptr)filed_ptr, item, occ);
    }

    return result_code::ok;
}

result_code
property::deserialize_update_item(reflection::property& p,
                                  blob_ptr ptr,
                                  const serialization::conteiner& jc,
                                  core::object_load_context& occ)
{
    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    if (!jc[p.name])
    {
        return result_code::doesnt_exist;
    }

    AGEA_check(p.rtype->deserialization_with_proto, "Should be not a NULL");

    p.rtype->deserialization_with_proto(ptr, jc[p.name], occ);
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

    // AGEA_check(context.p->types_compare_handler, "Should be not a NULL");

    return context.p->rtype->compare(src_ptr, dst_ptr);
}

}  // namespace reflection
}  // namespace agea

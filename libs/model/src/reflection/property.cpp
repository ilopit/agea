#include "model/reflection/property.h"

#include <inttypes.h>

#include "model/assets/material.h"
#include "model/assets/mesh.h"
#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/caches_map.h"

#include "utils/agea_log.h"

#include "serialization/serialization.h"

#include <array>

namespace agea
{
namespace reflection
{

static volatile bool initialize = property_convertes::init();

result_code
property::save_to_string(property& prop, blob_ptr ptr, fixed_size_buffer& str)
{
    if (prop.type.type == utils::agea_type::id::t_nan)
    {
        return result_code::failed;
    }

    auto fixed_ptr = reflection::reduce_ptr(ptr + prop.offset, prop.type.is_ptr);
    auto idx = (size_t)prop.type.type;

    return property_convertes::readers()[idx](fixed_ptr, str);
}

result_code
property::load_from_string(blob_ptr ptr, const std::string& str)
{
    if (type.type == utils::agea_type::id::t_nan)
    {
        return result_code::failed;
    }

    auto fixed_ptr = reduce_ptr(ptr + offset, type.is_ptr);
    auto idx = (size_t)type.type;

    return property_convertes::writers()[idx](fixed_ptr, str);
}

result_code
property::save_to_string_with_hint(blob_ptr ptr, const std::string& h, fixed_size_buffer& buf)
{
    if (type.type != utils::agea_type::id::t_vec3)
    {
        return result_code::failed;
    }

    auto hitr = std::find(hints.begin(), hints.end(), h);

    if (hitr == hints.end())
    {
        return result_code::failed;
    }

    auto extra_offset = (hitr - hints.begin()) * sizeof(float);

    auto fixed_ptr = reduce_ptr(ptr + offset + extra_offset, type.is_ptr);
    auto idx = (size_t)utils::agea_type::id::t_f;

    return property_convertes::readers()[idx](fixed_ptr, buf);
}

result_code
property::load_from_string_with_hint(blob_ptr ptr, const std::string& hint, const std::string& str)
{
    if (type.type != utils::agea_type::id::t_vec3)
    {
        return result_code::failed;
    }

    auto hitr = std::find(hints.begin(), hints.end(), hint);

    if (hitr == hints.end())
    {
        return result_code::failed;
    }

    auto extra_offset = (hitr - hints.begin()) * sizeof(float);

    auto fixed_ptr = reduce_ptr(ptr + offset + extra_offset, type.is_ptr);

    return property_convertes::writers()[(size_t)utils::agea_type::id::t_f](fixed_ptr, str);
}

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
    AGEA_check(cxt.src_property->types_copy_handler, "Should be valid!");

    AGEA_check(cxt.src_property == cxt.dst_property, "Should be SAME properties!");
    AGEA_check(cxt.src_obj != cxt.dst_obj, "Should not be SAME objects!");

    AGEA_check(!cxt.src_property->type.is_collection, "Not supported!");

    auto from = ::agea::reflection::reduce_ptr(cxt.src_property->get_blob(*cxt.src_obj),
                                               cxt.src_property->type.is_ptr);
    auto to = ::agea::reflection::reduce_ptr(cxt.dst_property->get_blob(*cxt.dst_obj),
                                             cxt.dst_property->type.is_ptr);

    AGEA_check(cxt.dst_property->types_copy_handler, "Should never happens!");
    return cxt.dst_property->types_copy_handler(*cxt.src_obj, *cxt.dst_obj, from, to, *cxt.occ);
}

result_code
property::default_prototype(property_prototype_context& ctx)
{
    if (!ctx.src_property->types_copy_handler)
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

            AGEA_check(ctx.dst_property->types_copy_handler, "Should never happens!");
            ctx.dst_property->types_copy_handler(*ctx.src_obj, *ctx.dst_obj, from, to, *ctx.occ);
        }
    }
    return result_code::ok;
}

agea::blob_ptr
property::get_blob(model::smart_object& obj)
{
    return obj.as_blob() + offset;
}

result_code
property::deserialize_update(reflection::property& p,
                             blob_ptr ptr,
                             const serialization::conteiner& jc,
                             model::object_load_context& occ)
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
                                 model::smart_object& obj,
                                 const serialization::conteiner& jc,
                                 model::object_load_context& occ)
{
    auto ptr = (blob_ptr)&obj;
    auto items = jc[p.name];
    auto items_size = items.size();
    auto& r = extract<std::vector<void*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }

    AGEA_check(p.types_deserialization_handler, "Should never happens!");

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];
        auto idx = item["order_idx"].as<std::uint32_t>();

        auto* filed_ptr = &r[idx];
        p.types_deserialization_handler(obj, (blob_ptr)filed_ptr, item, occ);
    }

    return result_code::ok;
}

result_code
property::serialize_collection(const reflection::property&,
                               const model::smart_object&,
                               serialization::conteiner&)
{
    return result_code::ok;
}

result_code
property::deserialize_item(reflection::property& p,
                           model::smart_object& obj,
                           const serialization::conteiner& jc,
                           model::object_load_context& occ)
{
    if (!jc[p.name])
    {
        ALOG_WARN("Unable to deserialize property [{0}][{1}:{2}] ", obj.get_id().cstr(),
                  obj.get_type_id().str(), p.name);
        return result_code::doesnt_exist;
    }

    auto ptr = obj.as_blob();

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    AGEA_check(p.types_deserialization_handler, "Should never happens!");

    return p.types_deserialization_handler(obj, ptr, jc[p.name], occ);
}

result_code
property::serialize_item(const reflection::property& p,
                         const model::smart_object& obj,
                         serialization::conteiner& sc)
{
    auto ptr = (blob_ptr)&obj;

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    serialization::conteiner c;

    AGEA_check(p.types_serialization_handler, "Should never happens!");
    p.types_serialization_handler(obj, ptr, c);

    sc[p.name] = c;

    return result_code::ok;
}

result_code
property::deserialize_update_collection(reflection::property& p,
                                        blob_ptr ptr,
                                        const serialization::conteiner& jc,
                                        model::object_load_context& occ)
{
    auto items = jc[p.name];
    auto items_size = items.size();
    auto& r = extract<std::vector<model::component*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }
    AGEA_check(p.types_update_handler, "Should never happens!");
    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];

        auto* filed_ptr = &r[i];
        p.types_update_handler((blob_ptr)filed_ptr, item, occ);
    }

    return result_code::ok;
}

result_code
property::deserialize_update_item(reflection::property& p,
                                  blob_ptr ptr,
                                  const serialization::conteiner& jc,
                                  model::object_load_context& occ)
{
    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    if (!jc[p.name])
    {
        return result_code::doesnt_exist;
    }

    AGEA_check(p.types_update_handler, "Should be not a NULL");

    p.types_update_handler(ptr, jc[p.name], occ);
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

    AGEA_check(context.p->types_compare_handler, "Should be not a NULL");

    return context.p->types_compare_handler(src_ptr, dst_ptr);
}

bool
property_convertes::init()
{
    // clang-format off

    readers().resize((size_t)utils::agea_type::id::t_last, nullptr);
    writers().resize((size_t)utils::agea_type::id::t_last, nullptr);

    readers()[(size_t)utils::agea_type::id::t_str]      = property_convertes::read_t_str;
    writers()[(size_t)utils::agea_type::id::t_str]      = property_convertes::write_t_str;

    readers()[(size_t)utils::agea_type::id::t_bool]     = property_convertes::read_t_bool;
    writers()[(size_t)utils::agea_type::id::t_bool]     = property_convertes::write_t_bool;

    readers()[(size_t)utils::agea_type::id::t_i8]       = property_convertes::read_t_i8;
    writers()[(size_t)utils::agea_type::id::t_i8]       = property_convertes::write_t_i8;
                                                  
    readers()[(size_t)utils::agea_type::id::t_i16]      = property_convertes::read_t_i16;
    writers()[(size_t)utils::agea_type::id::t_i16]      = property_convertes::write_t_i16;
                                                  
    readers()[(size_t)utils::agea_type::id::t_i32]      = property_convertes::read_t_i32;
    writers()[(size_t)utils::agea_type::id::t_i32]      = property_convertes::write_t_i32;
                                                  
    readers()[(size_t)utils::agea_type::id::t_i64]      = property_convertes::read_t_i64;
    writers()[(size_t)utils::agea_type::id::t_i64]      = property_convertes::write_t_i64;
                                                  
    readers()[(size_t)utils::agea_type::id::t_u8]       = property_convertes::read_t_u8;
    writers()[(size_t)utils::agea_type::id::t_u8]       = property_convertes::write_t_u8;
                                                  
    readers()[(size_t)utils::agea_type::id::t_u16]      = property_convertes::read_t_u16;
    writers()[(size_t)utils::agea_type::id::t_u16]      = property_convertes::write_t_u16;
                                                  
    readers()[(size_t)utils::agea_type::id::t_u32]      = property_convertes::read_t_u32;
    writers()[(size_t)utils::agea_type::id::t_u32]      = property_convertes::write_t_u32;
                                                  
    readers()[(size_t)utils::agea_type::id::t_u64]      = property_convertes::read_t_u64;
    writers()[(size_t)utils::agea_type::id::t_u64]      = property_convertes::write_t_u64;
                                                  
    readers()[(size_t)utils::agea_type::id::t_f]        = property_convertes::read_t_f;
    writers()[(size_t)utils::agea_type::id::t_f]        = property_convertes::write_t_f;
                                                  
    readers()[(size_t)utils::agea_type::id::t_d]        = property_convertes::read_t_d;
    writers()[(size_t)utils::agea_type::id::t_d]        = property_convertes::write_t_d;
                                                  
    readers()[(size_t)utils::agea_type::id::t_vec3]     = property_convertes::read_t_vec3;
    writers()[(size_t)utils::agea_type::id::t_vec3]     = property_convertes::write_t_vec3;
                                                  
    readers()[(size_t)utils::agea_type::id::t_mat]      = property_convertes::read_t_mat;
    writers()[(size_t)utils::agea_type::id::t_mat]      = property_convertes::write_t_mat;
                                                  
    readers()[(size_t)utils::agea_type::id::t_msh]      = property_convertes::read_t_msh;
    writers()[(size_t)utils::agea_type::id::t_msh]      = property_convertes::write_t_msh;

    /// ////////////////////////////////////////////////////////////////////////

    // clang-format on

    return true;
}

result_code
property_convertes::read_t_str(AGEA_read_from_property_args)
{
    auto& str = extract<std::string>(ptr);

    memcpy(buf.data(), str.data(), str.size());

    buf[str.size()] = '\0';

    return result_code::ok;
}

result_code
property_convertes::write_t_str(blob_ptr ptr, const std::string& str)
{
    extract<std::string>(ptr) = str;
    return result_code::ok;
}

// Bool

result_code
property_convertes::read_t_bool(AGEA_read_from_property_args)
{
    auto t = extract<bool>(ptr);

    const std::string str = (t ? "true" : "false");

    memcpy(buf.data(), str.data(), str.size());

    buf[str.size()] = '\0';

    return result_code::ok;
}

result_code
property_convertes::write_t_bool(blob_ptr ptr, const std::string& str)
{
    char c[8] = {0};
    auto& t = extract<bool>(ptr);

    sscanf_s(str.c_str(), "%s", c, 8);

    if (strcmp(c, "true") == 0)
    {
        t = true;
    }
    else if (strcmp(c, "false") == 0)
    {
        t = false;
    }
    else
    {
        return result_code::failed;
    }

    return result_code::ok;
}

// I8

result_code
property_convertes::read_t_i8(AGEA_read_from_property_args)
{
    auto t = extract<int8_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIi8 "", t);

    return result_code::ok;
}

result_code
property_convertes::write_t_i8(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int8_t>(ptr);

    sscanf_s(str.data(), "%" PRIi8 "", &t);

    return result_code::ok;
}

// I16

result_code
property_convertes::read_t_i16(AGEA_read_from_property_args)
{
    auto t = extract<int16_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIi16 "", t);

    return result_code::ok;
}

result_code
property_convertes::write_t_i16(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int16_t>(ptr);

    sscanf_s(str.data(), "%" PRIi16 "", &t);
    return result_code::ok;
}

// I32

result_code
property_convertes::read_t_i32(AGEA_read_from_property_args)
{
    auto t = extract<int32_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIi32, t);

    return result_code::ok;
}

result_code
property_convertes::write_t_i32(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int32_t>(ptr);

    sscanf_s(str.data(), "%" PRIi32, &t);
    return result_code::ok;
}

// I64

result_code
property_convertes::read_t_i64(AGEA_read_from_property_args)
{
    auto t = extract<int64_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIi64 "", t);

    return result_code::ok;
}

result_code
property_convertes::write_t_i64(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int64_t>(ptr);

    sscanf_s(str.data(), "%" PRIi64 "", &t);
    return result_code::ok;
}

// U8

result_code
property_convertes::read_t_u8(AGEA_read_from_property_args)
{
    auto t = extract<uint8_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIu8 "", t);

    return result_code::ok;
}

result_code
property_convertes::write_t_u8(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint8_t>(ptr);

    sscanf_s(str.data(), "%" PRIu8 "", &t);
    return result_code::ok;
}

// U16

result_code
property_convertes::read_t_u16(AGEA_read_from_property_args)
{
    auto t = extract<uint16_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIu16 "", t);

    return result_code::ok;
}

result_code
property_convertes::write_t_u16(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint16_t>(ptr);

    sscanf_s(str.data(), "%" PRIu16 "", &t);
    return result_code::ok;
}

// U32

result_code
property_convertes::read_t_u32(AGEA_read_from_property_args)
{
    auto t = extract<uint32_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIu32 "", t);

    return result_code::ok;
}

result_code
property_convertes::write_t_u32(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint32_t>(ptr);

    sscanf_s(str.data(), "%" PRIu32 "", &t);
    return result_code::ok;
}

// U64
result_code
property_convertes::write_t_u64(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint64_t>(ptr);

    sscanf_s(str.data(), "%" PRIu64 "", &t);
    return result_code::ok;
}

result_code
property_convertes::read_t_u64(AGEA_read_from_property_args)
{
    auto t = extract<uint64_t>(ptr);

    sprintf_s(buf.data(), buf.size(), "%" PRIu64 "", t);
    return result_code::ok;
}

// Float

result_code
property_convertes::read_t_f(AGEA_read_from_property_args)
{
    auto t = extract<float>(ptr);

    sprintf_s(buf.data(), buf.size(), "%f", t);
    return result_code::ok;
}

result_code
property_convertes::write_t_f(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<float>(ptr);

    sscanf_s(str.data(), "%f", &t);
    return result_code::ok;
}

// Double

result_code
property_convertes::read_t_d(AGEA_read_from_property_args)
{
    auto t = extract<double>(ptr);

    sprintf_s(buf.data(), buf.size(), "%lf", t);
    return result_code::ok;
}

result_code
property_convertes::write_t_d(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<double>(ptr);

    sscanf_s(str.data(), "%lf", &t);
    return result_code::ok;
}

// Vec3

result_code
property_convertes::read_t_vec3(AGEA_read_from_property_args)
{
    auto t = extract<glm::vec3>(ptr);
    sprintf_s(buf.data(), buf.size(), "%f %f %f", t.x, t.y, t.z);

    return result_code::ok;
}

result_code
property_convertes::write_t_vec3(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<glm::vec3>(ptr);
    sscanf_s(str.data(), "%f %f %f", &t.x, &t.y, &t.z);
    return result_code::ok;
}

// Material

result_code
property_convertes::read_t_mat(AGEA_read_from_property_args)
{
    auto& t = extract<model::material*>(ptr);
    sprintf_s(buf.data(), buf.size(), "%s", t->get_id().cstr());

    return result_code::ok;
}

result_code
property_convertes::write_t_mat(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<model::material*>(ptr);

    auto mat = glob::materials_cache::get()->get_item(AID(str));

    if (!mat)
    {
        return result_code::failed;
    }

    t = mat;

    return result_code::ok;
}

// Mesh

result_code
property_convertes::read_t_msh(AGEA_read_from_property_args)
{
    auto t = extract<std::shared_ptr<model::mesh>>(ptr);
    sprintf_s(buf.data(), buf.size(), "%s", t->get_id().cstr());

    return result_code::ok;
}

result_code
property_convertes::write_t_msh(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<model::mesh*>(ptr);

    auto mat = glob::meshes_cache::get()->get_item(AID(str));

    if (!mat)
    {
        return result_code::failed;
    }

    t = mat;

    return result_code::ok;
}

}  // namespace reflection
}  // namespace agea

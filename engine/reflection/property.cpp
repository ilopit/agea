#include "reflection/property.h"

#include <inttypes.h>

#include "model/rendering/material.h"
#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"
#include "utils/agea_log.h"

namespace agea
{
namespace reflection
{

static volatile bool initialize = property_convertes::init();

bool
property::save_to_string(property& prop, blob_ptr ptr, std::string& str)
{
    if (prop.type.type == property_type::t_nan)
    {
        return false;
    }

    auto fixed_ptr = reflection::reduce_ptr(ptr + prop.offset, prop.type.is_ptr);
    auto idx = (size_t)prop.type.type;

    return property_convertes::readers()[idx](fixed_ptr, str);
}

bool
property::load_from_string(blob_ptr ptr, const std::string& str)
{
    if (type.type == property_type::t_nan)
    {
        return false;
    }

    auto fixed_ptr = reduce_ptr(ptr + offset, type.is_ptr);
    auto idx = (size_t)type.type;

    return property_convertes::writers()[idx](fixed_ptr, str);
}

bool
property::save_to_string_with_hint(blob_ptr ptr, const std::string& h, std::string& buf)
{
    if (type.type != property_type::t_vec3)
    {
        return false;
    }

    auto hitr = std::find(hints.begin(), hints.end(), h);

    if (hitr == hints.end())
    {
        return false;
    }

    auto extra_offset = (hitr - hints.begin()) * sizeof(float);

    auto fixed_ptr = reduce_ptr(ptr + offset + extra_offset, type.is_ptr);
    auto idx = (size_t)property_type::t_f;

    return property_convertes::readers()[idx](fixed_ptr, buf);
}

bool
property::load_from_string_with_hint(blob_ptr ptr, const std::string& hint, const std::string& str)
{
    if (type.type != property_type::t_vec3)
    {
        return false;
    }

    auto hitr = std::find(hints.begin(), hints.end(), hint);

    if (hitr == hints.end())
    {
        return false;
    }

    auto extra_offset = (hitr - hints.begin()) * sizeof(float);

    auto fixed_ptr = reduce_ptr(ptr + offset + extra_offset, type.is_ptr);

    return property_convertes::writers()[(size_t)property_type::t_f](fixed_ptr, str);
}

bool
property::copy(property& from_property,
               property& to_property,
               model::smart_object& src_obj,
               model::smart_object& dst_obj,
               model::object_constructor_context& occ)
{
    if (!from_property.copy_handler)
    {
        return true;
    }

    AGEA_check(&to_property == &from_property, "Should be SAME properties!");
    AGEA_check(&src_obj != &dst_obj, "Should not be SAME objects!");

    if (to_property.type.is_collection)
    {
        auto& fromcol =
            extract<std::vector<model::component*>>((blob_ptr)&src_obj + from_property.offset);
        auto& tocol =
            extract<std::vector<model::component*>>((blob_ptr)&dst_obj + to_property.offset);

        tocol.resize(fromcol.size());

        for (int i = 0; i < fromcol.size(); ++i)
        {
            from_property.copy_handler(src_obj, dst_obj, (blob_ptr)&fromcol[i], (blob_ptr)&tocol[i],
                                       occ);
        }
    }
    else
    {
        auto from = ::agea::reflection::reduce_ptr((blob_ptr)&src_obj + from_property.offset,
                                                   from_property.type.is_ptr);
        auto to = ::agea::reflection::reduce_ptr((blob_ptr)&dst_obj + to_property.offset,
                                                 from_property.type.is_ptr);
        from_property.copy_handler(src_obj, dst_obj, from, to, occ);
    }
    return true;
}

bool
property::serialize(reflection::property&,
                    blob_ptr,
                    serialization::json_conteiner&,
                    model::object_constructor_context&)
{
    return true;
}

bool
property::deserialize(reflection::property& p,
                      model::smart_object& obj,
                      serialization::json_conteiner& jc,
                      model::object_constructor_context& occ)
{
    if (p.type.is_collection)
    {
        return deserialize_collection(p, obj, jc, occ);
    }
    else
    {
        return deserialize_item(p, obj, jc, occ);
    }
}

bool
property::deserialize_update(reflection::property& p,
                             blob_ptr ptr,
                             serialization::json_conteiner& jc,
                             model::object_constructor_context& occ)
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

bool
property::deserialize_collection(reflection::property& p,
                                 model::smart_object& obj,
                                 serialization::json_conteiner& jc,
                                 model::object_constructor_context& occ)
{
    auto ptr = (blob_ptr)&obj;
    auto& items = jc[p.name];
    auto items_size = items.size();
    auto& r = extract<std::vector<void*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto& item = items[i];
        auto idx = item["order_idx"].asUInt();

        auto* filed_ptr = &r[idx];
        p.serialization_handler((blob_ptr)filed_ptr, item, occ);
    }

    return true;
}

bool
property::deserialize_item(reflection::property& p,
                           model::smart_object& obj,
                           serialization::json_conteiner& jc,
                           model::object_constructor_context& occ)
{
    if (!jc.isMember(p.name))
    {
        ALOG_ERROR("Unable to deserialize property [{0}:{1}] ", obj.type_id(), p.name);
        return false;
    }

    auto ptr = (blob_ptr)&obj;

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    p.serialization_handler(ptr, jc[p.name], occ);

    return true;
}

bool
property::deserialize_update_collection(reflection::property& p,
                                        blob_ptr ptr,
                                        serialization::json_conteiner& jc,
                                        model::object_constructor_context& occ)
{
    auto& items = jc[p.name];
    auto items_size = items.size();
    auto& r = extract<std::vector<model::component*>>(ptr + p.offset);

    if (r.empty())
    {
        r.resize(items_size);
    }

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto& item = items[i];
        auto idx = item["order_idx"].asUInt();

        auto* filed_ptr = &r[idx];
        p.update_handler((blob_ptr)filed_ptr, item, occ);
    }

    return true;
}

bool
property::deserialize_update_item(reflection::property& p,
                                  blob_ptr ptr,
                                  serialization::json_conteiner& jc,
                                  model::object_constructor_context& occ)
{
    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);

    if (!jc.isMember(p.name))
    {
        return false;
    }

    p.update_handler(ptr, jc[p.name], occ);
    return true;
}

bool
property_convertes::init()
{
    // clang-format off

    readers().resize((size_t)property_type::t_last, nullptr);
    writers().resize((size_t)property_type::t_last, nullptr);

    readers()[(size_t)property_type::t_str]      = property_convertes::read_t_str;
    writers()[(size_t)property_type::t_str]      = property_convertes::write_t_str;

    readers()[(size_t)property_type::t_bool]     = property_convertes::read_t_bool;
    writers()[(size_t)property_type::t_bool]     = property_convertes::write_t_bool;

    readers()[(size_t)property_type::t_i8]       = property_convertes::read_t_i8;
    writers()[(size_t)property_type::t_i8]       = property_convertes::write_t_i8;
                                                  
    readers()[(size_t)property_type::t_i16]      = property_convertes::read_t_i16;
    writers()[(size_t)property_type::t_i16]      = property_convertes::write_t_i16;
                                                  
    readers()[(size_t)property_type::t_i32]      = property_convertes::read_t_i32;
    writers()[(size_t)property_type::t_i32]      = property_convertes::write_t_i32;
                                                  
    readers()[(size_t)property_type::t_i64]      = property_convertes::read_t_i64;
    writers()[(size_t)property_type::t_i64]      = property_convertes::write_t_i64;
                                                  
    readers()[(size_t)property_type::t_u8]       = property_convertes::read_t_u8;
    writers()[(size_t)property_type::t_u8]       = property_convertes::write_t_u8;
                                                  
    readers()[(size_t)property_type::t_u16]      = property_convertes::read_t_u16;
    writers()[(size_t)property_type::t_u16]      = property_convertes::write_t_u16;
                                                  
    readers()[(size_t)property_type::t_u32]      = property_convertes::read_t_u32;
    writers()[(size_t)property_type::t_u32]      = property_convertes::write_t_u32;
                                                  
    readers()[(size_t)property_type::t_u64]      = property_convertes::read_t_u64;
    writers()[(size_t)property_type::t_u64]      = property_convertes::write_t_u64;
                                                  
    readers()[(size_t)property_type::t_f]        = property_convertes::read_t_f;
    writers()[(size_t)property_type::t_f]        = property_convertes::write_t_f;
                                                  
    readers()[(size_t)property_type::t_d]        = property_convertes::read_t_d;
    writers()[(size_t)property_type::t_d]        = property_convertes::write_t_d;
                                                  
    readers()[(size_t)property_type::t_vec3]     = property_convertes::read_t_vec3;
    writers()[(size_t)property_type::t_vec3]     = property_convertes::write_t_vec3;
                                                  
    readers()[(size_t)property_type::t_mat]      = property_convertes::read_t_mat;
    writers()[(size_t)property_type::t_mat]      = property_convertes::write_t_mat;
                                                  
    readers()[(size_t)property_type::t_msh]      = property_convertes::read_t_msh;
    writers()[(size_t)property_type::t_msh]      = property_convertes::write_t_msh;

    /// ////////////////////////////////////////////////////////////////////////

    // clang-format on

    return true;
}

bool
property_convertes::read_t_str(blob_ptr ptr, std::string& str)
{
    str = extract<std::string>(ptr);

    return true;
}

bool
property_convertes::write_t_str(blob_ptr ptr, const std::string& str)
{
    extract<std::string>(ptr) = str;
    return true;
}

// Bool

bool
property_convertes::read_t_bool(blob_ptr ptr, std::string& str)
{
    auto t = extract<bool>(ptr);

    str = (t ? "true" : "false");

    return true;
}

bool
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
        return false;
    }

    return true;
}

// I8

bool
property_convertes::read_t_i8(blob_ptr ptr, std::string& str)
{
    auto t = extract<int8_t>(ptr);

    sprintf(str.data(), "%" PRIi8 "", t);

    return true;
}

bool
property_convertes::write_t_i8(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int8_t>(ptr);

    sscanf_s(str.data(), "%" PRIi8 "", &t);

    return true;
}

// I16

bool
property_convertes::read_t_i16(blob_ptr ptr, std::string& str)
{
    auto t = extract<int16_t>(ptr);

    sprintf(str.data(), "%" PRIi16 "", t);

    return true;
}

bool
property_convertes::write_t_i16(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int16_t>(ptr);

    sscanf_s(str.data(), "%" PRIi16 "", &t);
    return true;
}

// I32

bool
property_convertes::read_t_i32(blob_ptr ptr, std::string& str)
{
    auto t = extract<int32_t>(ptr);

    sscanf_s(str.c_str(), "%" PRIi32, &t);

    return true;
}

bool
property_convertes::write_t_i32(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int32_t>(ptr);

    sscanf_s(str.data(), "%" PRIi32, &t);
    return true;
}

// I64

bool
property_convertes::read_t_i64(blob_ptr ptr, std::string& str)
{
    auto t = extract<int64_t>(ptr);

    sprintf(str.data(), "%" PRIi64 "", t);

    return true;
}

bool
property_convertes::write_t_i64(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<int64_t>(ptr);

    sscanf_s(str.data(), "%" PRIi64 "", &t);
    return true;
}

// U8

bool
property_convertes::read_t_u8(blob_ptr ptr, std::string& str)
{
    auto t = extract<uint8_t>(ptr);

    sprintf(str.data(), "%" PRIu8 "", t);

    return true;
}

bool
property_convertes::write_t_u8(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint8_t>(ptr);

    sscanf_s(str.data(), "%" PRIu8 "", &t);
    return true;
}

// U16

bool
property_convertes::read_t_u16(blob_ptr ptr, std::string& str)
{
    auto t = extract<uint16_t>(ptr);

    sprintf(str.data(), "%" PRIu16 "", t);

    return true;
}

bool
property_convertes::write_t_u16(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint16_t>(ptr);

    sscanf_s(str.data(), "%" PRIu16 "", &t);
    return true;
}

// U32

bool
property_convertes::read_t_u32(blob_ptr ptr, std::string& str)
{
    auto t = extract<uint32_t>(ptr);

    sprintf(str.data(), "%" PRIu32 "", t);

    return true;
}

bool
property_convertes::write_t_u32(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint32_t>(ptr);

    sscanf_s(str.data(), "%" PRIu32 "", &t);
    return true;
}

// U64
bool
property_convertes::write_t_u64(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<uint64_t>(ptr);

    sscanf_s(str.data(), "%" PRIu64 "", &t);
    return true;
}

bool
property_convertes::read_t_u64(blob_ptr ptr, std::string& str)
{
    auto t = extract<uint64_t>(ptr);

    sprintf(str.data(), "%" PRIu64 "", t);
    return true;
}

// Float

bool
property_convertes::read_t_f(blob_ptr ptr, std::string& str)
{
    auto t = extract<float>(ptr);

    sprintf(str.data(), "%f", t);
    return true;
}

bool
property_convertes::write_t_f(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<float>(ptr);

    sscanf_s(str.data(), "%f", &t);
    return true;
}

// Double

bool
property_convertes::read_t_d(blob_ptr ptr, std::string& str)
{
    auto t = extract<double>(ptr);

    sprintf(str.data(), "%lf", t);
    return true;
}

bool
property_convertes::write_t_d(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<double>(ptr);

    sscanf_s(str.data(), "%lf", &t);
    return true;
}

// Vec3

bool
property_convertes::read_t_vec3(blob_ptr ptr, std::string& str)
{
    auto t = extract<glm::vec3>(ptr);
    sprintf(str.data(), "%f %f %f", t.x, t.y, t.z);

    return true;
}

bool
property_convertes::write_t_vec3(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<glm::vec3>(ptr);
    sscanf_s(str.data(), "%f %f %f", &t.x, &t.y, &t.z);
    return true;
}

// Material

bool
property_convertes::read_t_mat(blob_ptr ptr, std::string& str)
{
    auto& t = extract<std::shared_ptr<model::material>>(ptr);
    sprintf(str.data(), "%s", t->id().c_str());

    return true;
}

bool
property_convertes::write_t_mat(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<std::shared_ptr<model::material>>(ptr);

    auto mat = glob::materials_cache::get()->get(str);

    if (!mat)
    {
        return false;
    }

    t = mat;

    return true;
}

// Mesh

bool
property_convertes::read_t_msh(blob_ptr ptr, std::string& str)
{
    auto t = extract<std::shared_ptr<model::mesh>>(ptr);
    sprintf(str.data(), "%s", t->id().c_str());

    return true;
}

bool
property_convertes::write_t_msh(blob_ptr ptr, const std::string& str)
{
    auto& t = extract<std::shared_ptr<model::mesh>>(ptr);

    auto mat = glob::meshes_cache::get()->get(str);

    if (!mat)
    {
        return false;
    }

    t = mat;

    return true;
}

}  // namespace reflection
}  // namespace agea

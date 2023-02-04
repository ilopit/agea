#include "model/reflection/type_handlers/property_type_serialization_handlers.h"

#include "model/level.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/cache_set.h"
#include "model/caches/caches_map.h"
#include "model/caches/materials_cache.h"
#include "model/caches/objects_cache.h"
#include "model/object_load_context.h"
#include "model/caches/game_objects_cache.h"
#include "model/object_constructor.h"
#include "model/package.h"

#include "model/assets/mesh.h"
#include "model/assets/material.h"
#include "model/assets/texture.h"
#include "model/assets/shader_effect.h"

#include <serialization/serialization.h>

#include <utils/id.h>
#include <utils/string_utility.h>
#include <utils/buffer.h>
#include <utils/agea_log.h>

namespace agea
{
namespace reflection
{

namespace
{
template <typename T>
void
extract_field(blob_ptr ptr, const serialization::conteiner& jc)
{
    extract<T>(ptr) = jc.as<T>();
}

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  model::object_load_context& occ,
                  model::architype a_type)
{
    auto& field = reflection::extract<::agea::model::smart_object*>(ptr);

    const auto id = AID(jc.as<std::string>());

    return model::object_constructor::object_load_internal(id, occ, field);
}

}  // namespace

bool
property_type_serialization_handlers::init()
{
    using namespace reflection;
    // clang-format off
    serializers()  .resize((size_t)utils::agea_type::t_last, nullptr);
    deserializers().resize((size_t)utils::agea_type::t_last, nullptr);

    serializers()  [(size_t)utils::agea_type::t_str]  = serialize_t_str;
    deserializers()[(size_t)utils::agea_type::t_str]  = deserialize_t_str;

    serializers()  [(size_t)utils::agea_type::t_id]   = serialize_t_id;
    deserializers()[(size_t)utils::agea_type::t_id]   = deserialize_t_id;

    serializers()  [(size_t)utils::agea_type::t_bool] = serialize_t_bool;
    deserializers()[(size_t)utils::agea_type::t_bool] = deserialize_t_bool;

    serializers()  [(size_t)utils::agea_type::t_i8]   = serialize_t_i8;
    deserializers()[(size_t)utils::agea_type::t_i8]   = deserialize_t_i8;

    serializers()  [(size_t)utils::agea_type::t_i16]  = serialize_t_i16;
    deserializers()[(size_t)utils::agea_type::t_i16]  = deserialize_t_i16;

    serializers()  [(size_t)utils::agea_type::t_i32]  = serialize_t_i32;
    deserializers()[(size_t)utils::agea_type::t_i32]  = deserialize_t_i32;

    serializers()  [(size_t)utils::agea_type::t_i64]  = serialize_t_i64;
    deserializers()[(size_t)utils::agea_type::t_i64]  = deserialize_t_i64;

    serializers()  [(size_t)utils::agea_type::t_u8]   = serialize_t_u8;
    deserializers()[(size_t)utils::agea_type::t_u8]   = deserialize_t_u8;

    serializers()  [(size_t)utils::agea_type::t_u16]  = serialize_t_u16;
    deserializers()[(size_t)utils::agea_type::t_u16]  = deserialize_t_u16;

    serializers()  [(size_t)utils::agea_type::t_u32]  = serialize_t_u32;
    deserializers()[(size_t)utils::agea_type::t_u32]  = deserialize_t_u32;

    serializers()  [(size_t)utils::agea_type::t_u64]  = serialize_t_u64;
    deserializers()[(size_t)utils::agea_type::t_u64]  = deserialize_t_u64;

    serializers()  [(size_t)utils::agea_type::t_f]    = serialize_t_f;
    deserializers()[(size_t)utils::agea_type::t_f]    = deserialize_t_f;

    serializers()  [(size_t)utils::agea_type::t_d]    = serialize_t_d;
    deserializers()[(size_t)utils::agea_type::t_d]    = deserialize_t_d;

    serializers()  [(size_t)utils::agea_type::t_vec3] = serialize_t_vec3;
    deserializers()[(size_t)utils::agea_type::t_vec3] = deserialize_t_vec3;

    serializers()  [(size_t)utils::agea_type::t_txt]  = serialize_t_txt;
    deserializers()[(size_t)utils::agea_type::t_txt]  = deserialize_t_txt;

    serializers()  [(size_t)utils::agea_type::t_mat]  = serialize_t_mat;
    deserializers()[(size_t)utils::agea_type::t_mat]  = deserialize_t_mat;

    serializers()  [(size_t)utils::agea_type::t_msh]  = serialize_t_msh;
    deserializers()[(size_t)utils::agea_type::t_msh]  = deserialize_t_msh;

    serializers()  [(size_t)utils::agea_type::t_obj]  = serialize_t_obj;
    deserializers()[(size_t)utils::agea_type::t_obj]  = deserialize_t_obj;

    serializers()  [(size_t)utils::agea_type::t_com]  = serialize_t_com;
    deserializers()[(size_t)utils::agea_type::t_com]  = deserialize_t_com;

    serializers()[(size_t)utils::agea_type::t_se]       = serialize_t_se;
    deserializers()[(size_t)utils::agea_type::t_se]     = deserialize_t_se;
    
    serializers()  [(size_t)utils::agea_type::t_color]  = serialize_t_color;
    deserializers()[(size_t)utils::agea_type::t_color]  = deserialize_t_color;

    serializers()  [(size_t)utils::agea_type::t_buf]    = serialize_t_buf;
    deserializers()[(size_t)utils::agea_type::t_buf]    = deserialize_t_buf;
    // clang-format on

    return true;
}

// STR
result_code
property_type_serialization_handlers::serialize_t_str(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::pack_field<std::string>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_str(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::extract_field<std::string>(ptr, jc);
    return result_code::ok;
}

// ID
result_code
property_type_serialization_handlers::serialize_t_id(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    jc = extract<utils::id>(ptr).str();
    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_id(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    extract<utils::id>(ptr) = AID(jc.as<std::string>());
    return result_code::ok;
}

// Bool
result_code
property_type_serialization_handlers::serialize_t_bool(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<bool>(ptr, jc);

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_bool(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<bool>(ptr, jc);
    return result_code::ok;
}

// I8
result_code
property_type_serialization_handlers::serialize_t_i8(AGEA_serialization_args)
{
    AGEA_unused(ptr);

    reflection::pack_field<std::int8_t>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_i8(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::int8_t>(ptr) = jc.as<int8_t>();
    return result_code::ok;
}

// I16
result_code
property_type_serialization_handlers::serialize_t_i16(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::pack_field<std::int16_t>(ptr, jc);

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_i16(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::int16_t>(ptr) = jc.as<std::int16_t>();
    return result_code::ok;
}

// I32
result_code
property_type_serialization_handlers::serialize_t_i32(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::int32_t>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_i32(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int32_t>(ptr, jc);
    return result_code::ok;
}

// I64
result_code
property_type_serialization_handlers::serialize_t_i64(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::int64_t>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_i64(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int64_t>(ptr, jc);
    return result_code::ok;
}

// U8
result_code
property_type_serialization_handlers::serialize_t_u8(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    reflection::pack_field<std::uint8_t>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_u8(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::uint8_t>(ptr) = jc.as<std::uint8_t>();
    return result_code::ok;
}

// U16
result_code
property_type_serialization_handlers::serialize_t_u16(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::uint16_t>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_u16(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::uint16_t>(ptr) = jc.as<std::uint16_t>();
    return result_code::ok;
}

// U32
result_code
property_type_serialization_handlers::serialize_t_u32(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::uint32_t>(ptr, jc);

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_u32(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::uint32_t>(ptr, jc);
    return result_code::ok;
}

// U64
result_code
property_type_serialization_handlers::serialize_t_u64(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::uint64_t>(ptr, jc);

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_u64(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::uint64_t>(ptr, jc);
    return result_code::ok;
}

// Float
result_code
property_type_serialization_handlers::serialize_t_f(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<float>(ptr, jc);

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_f(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<float>(ptr, jc);
    return result_code::ok;
}

// Double
result_code
property_type_serialization_handlers::serialize_t_d(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<double>(ptr, jc);

    return result_code::ok;
}
result_code
property_type_serialization_handlers::deserialize_t_d(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<double>(ptr, jc);
    return result_code::ok;
}

// Vec3
result_code
property_type_serialization_handlers::serialize_t_vec3(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::extract<model::vec3>(ptr);

    jc["x"] = field.x;
    jc["y"] = field.y;
    jc["z"] = field.z;

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_vec3(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<model::vec3>(ptr);

    field.x = jc["x"].as<float>();
    field.y = jc["y"].as<float>();
    field.z = jc["z"].as<float>();

    return result_code::ok;
}

// Texture
result_code
property_type_serialization_handlers::serialize_t_txt(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::texture*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_txt(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::texture);
}

// Material
result_code
property_type_serialization_handlers::serialize_t_mat(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::material*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_mat(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::material);
}

// Mesh
result_code
property_type_serialization_handlers::serialize_t_msh(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::mesh*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_msh(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::mesh);
}

result_code
property_type_serialization_handlers::serialize_t_obj(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);

    jc["id"] = field->get_id().str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_obj(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);

    auto id = AID(jc["id"].as<std::string>());

    auto pstr = glob::class_objects_cache::get()->get_item(id);

    field = pstr;

    return result_code::ok;
}

result_code
property_type_serialization_handlers::serialize_t_se(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_se(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::shader_effect);
}

result_code
property_type_serialization_handlers::serialize_t_com(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);

    jc["id"] = field->get_id().str();
    jc["object_class"] = field->get_class_obj()->get_id().str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_com(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    AGEA_not_implemented;

    return result_code::ok;
}

result_code
property_type_serialization_handlers::serialize_t_color(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::extract<::agea::model::color>(ptr);

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_color(AGEA_deserialization_args)
{
    auto str_color = jc.as<std::string>();

    if (str_color.size() != 0)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    uint8_t rgba[4] = {0, 0, 0, 255};

    agea::string_utils::convert_hext_string_to_bytes(8, str_color.data(), rgba);

    auto& field = reflection::extract<::agea::model::color>(ptr);

    field.m_data.r = rgba[0] ? (rgba[0] / 255.f) : 0.f;
    field.m_data.g = rgba[1] ? (rgba[1] / 255.f) : 0.f;
    field.m_data.b = rgba[2] ? (rgba[2] / 255.f) : 0.f;
    field.m_data.a = rgba[3] ? (rgba[3] / 255.f) : 0.f;

    return result_code::ok;
}

result_code
property_type_serialization_handlers::serialize_t_buf(AGEA_serialization_args)
{
    auto& field = reflection::extract<::agea::utils::buffer>(ptr);

    auto package_path = obj.get_package()->get_relative_path(field.get_file());

    if (!utils::buffer::save(field))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    jc = package_path.str();

    return result_code::ok;
}

result_code
property_type_serialization_handlers::deserialize_t_buf(AGEA_deserialization_args)
{
    auto rel_path = APATH(jc.as<std::string>());

    utils::path package_path;

    if (!occ.make_full_path(rel_path, package_path))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto& f = reflection::extract<::agea::utils::buffer>(ptr);
    f.set_file(package_path);

    if (!utils::buffer::load(package_path, f))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return result_code::ok;
}

}  // namespace reflection
}  // namespace agea
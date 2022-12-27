#include "model/reflection/type_handlers/property_type_serialization_update_handlers.h"

#include "model/level.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/materials_cache.h"
#include "model/object_constructor.h"
#include "model/core_types/vec3.h"

#include "utils/agea_log.h"

#include "serialization/serialization.h"

namespace agea
{
namespace reflection
{

template <typename T>
void
extract_field(blob_ptr ptr, const serialization::conteiner& jc)
{
    extract<T>(ptr) = jc.as<T>();
}

bool
property_type_serialization_update_handlers::init()
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

    serializers()  [(size_t)utils::agea_type::t_obj]  = serialize_t_obj;
    deserializers()[(size_t)utils::agea_type::t_obj]  = deserialize_t_obj;

    serializers()[(size_t)utils::agea_type::t_com]    = serialize_t_com;
    deserializers()[(size_t)utils::agea_type::t_com]  = deserialize_t_com;
    // clang-format on

    return true;
}

// STR
bool
property_type_serialization_update_handlers::serialize_t_str(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_str(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::string>(ptr, jc);
    return true;
}

// STR
bool
property_type_serialization_update_handlers::serialize_t_id(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_id(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    extract<utils::id>(ptr) = AID(jc.as<std::string>());
    return true;
}

// Bool
bool
property_type_serialization_update_handlers::serialize_t_bool(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_bool(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<bool>(ptr, jc);
    return true;
}

// I8
bool
property_type_serialization_update_handlers::serialize_t_i8(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_i8(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract<std::int8_t>(ptr) = jc.as<std::int8_t>();
    return true;
}

// I16
bool
property_type_serialization_update_handlers::serialize_t_i16(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_i16(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract<std::int16_t>(ptr) = jc.as<std::int16_t>();
    return true;
}

// I32
bool
property_type_serialization_update_handlers::serialize_t_i32(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_i32(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int32_t>(ptr, jc);
    return true;
}

// I64
bool
property_type_serialization_update_handlers::serialize_t_i64(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_i64(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int64_t>(ptr, jc);
    return true;
}

// U8
bool
property_type_serialization_update_handlers::serialize_t_u8(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_u8(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract<std::uint8_t>(ptr) = jc.as<std::uint8_t>();
    return true;
}

// U16
bool
property_type_serialization_update_handlers::serialize_t_u16(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_u16(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract<std::uint16_t>(ptr) = jc.as<std::uint16_t>();
    return true;
}

// U32
bool
property_type_serialization_update_handlers::serialize_t_u32(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_u32(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int32_t>(ptr, jc);
    return true;
}

// U64
bool
property_type_serialization_update_handlers::serialize_t_u64(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_u64(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::uint64_t>(ptr, jc);
    return true;
}

// Float
bool
property_type_serialization_update_handlers::serialize_t_f(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_f(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<float>(ptr, jc);
    return true;
}

// Double
bool
property_type_serialization_update_handlers::serialize_t_d(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}
bool
property_type_serialization_update_handlers::deserialize_t_d(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    reflection::extract_field<double>(ptr, jc);
    return true;
}

// Vec3
bool
property_type_serialization_update_handlers::serialize_t_vec3(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_vec3(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<model::vec3>(ptr);

    field.x = jc["x"].as<float>();
    field.y = jc["y"].as<float>();
    field.z = jc["z"].as<float>();

    return true;
}

bool
property_type_serialization_update_handlers::serialize_t_obj(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_obj(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<::agea::model::smart_object*>(ptr);

    ::agea::model::object_constructor::update_object_properties(*field, jc, occ);

    return true;
}

bool
property_type_serialization_update_handlers::serialize_t_com(AGEA_deserialization_update_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    return true;
}

bool
property_type_serialization_update_handlers::deserialize_t_com(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<::agea::model::smart_object*>(ptr);

    ::agea::model::object_constructor::update_object_properties(*field, jc, occ);

    return true;
}

}  // namespace reflection
}  // namespace agea
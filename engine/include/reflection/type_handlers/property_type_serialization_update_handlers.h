#pragma once

#include "core/agea_minimal.h"

#include "reflection/property_utils.h"

#include "serialization/serialization_fwds.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{

struct property_type_serialization_update_handlers
{
    static std::vector<reflection::type_serialization_update_handler>&
    serializers()
    {
        static std::vector<reflection::type_serialization_update_handler> s_serializers;
        return s_serializers;
    }

    static std::vector<reflection::type_serialization_update_handler>&
    deserializers()
    {
        static std::vector<reflection::type_serialization_update_handler> s_deserializers;
        return s_deserializers;
    }

    static bool
    init();

    // STR
    static bool serialize_t_str(AGEA_deserialization_update_args);
    static bool deserialize_t_str(AGEA_deserialization_update_args);

    // STR
    static bool serialize_t_id(AGEA_deserialization_update_args);
    static bool deserialize_t_id(AGEA_deserialization_update_args);

    // Bool
    static bool serialize_t_bool(AGEA_deserialization_update_args);
    static bool deserialize_t_bool(AGEA_deserialization_update_args);

    // I8
    static bool serialize_t_i8(AGEA_deserialization_update_args);
    static bool deserialize_t_i8(AGEA_deserialization_update_args);

    // I16
    static bool serialize_t_i16(AGEA_deserialization_update_args);
    static bool deserialize_t_i16(AGEA_deserialization_update_args);

    // I32
    static bool serialize_t_i32(AGEA_deserialization_update_args);
    static bool deserialize_t_i32(AGEA_deserialization_update_args);

    // I64
    static bool serialize_t_i64(AGEA_deserialization_update_args);
    static bool deserialize_t_i64(AGEA_deserialization_update_args);

    // U8
    static bool serialize_t_u8(AGEA_deserialization_update_args);
    static bool deserialize_t_u8(AGEA_deserialization_update_args);

    // U16
    static bool serialize_t_u16(AGEA_deserialization_update_args);
    static bool deserialize_t_u16(AGEA_deserialization_update_args);

    // U32
    static bool serialize_t_u32(AGEA_deserialization_update_args);
    static bool deserialize_t_u32(AGEA_deserialization_update_args);

    // U64
    static bool serialize_t_u64(AGEA_deserialization_update_args);
    static bool deserialize_t_u64(AGEA_deserialization_update_args);

    // Float
    static bool serialize_t_f(AGEA_deserialization_update_args);
    static bool deserialize_t_f(AGEA_deserialization_update_args);

    // Double
    static bool serialize_t_d(AGEA_deserialization_update_args);
    static bool deserialize_t_d(AGEA_deserialization_update_args);

    // Vec3
    static bool serialize_t_vec3(AGEA_deserialization_update_args);
    static bool deserialize_t_vec3(AGEA_deserialization_update_args);

    // Obj
    static bool serialize_t_obj(AGEA_deserialization_update_args);
    static bool deserialize_t_obj(AGEA_deserialization_update_args);

    // Com
    static bool serialize_t_com(AGEA_deserialization_update_args);
    static bool deserialize_t_com(AGEA_deserialization_update_args);
};

}  // namespace reflection
}  // namespace agea
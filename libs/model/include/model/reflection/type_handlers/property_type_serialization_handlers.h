#pragma once

#include "serialization/serialization_fwds.h"

#include "model/reflection/property_utils.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{

struct property_type_serialization_handlers
{
    static std::vector<reflection::type_serialization_handler>&
    serializers()
    {
        static std::vector<reflection::type_serialization_handler> s_serializers;
        return s_serializers;
    }

    static std::vector<reflection::type_deserialization_handler>&
    deserializers()
    {
        static std::vector<reflection::type_deserialization_handler> s_deserializers;
        return s_deserializers;
    }

    static bool
    init();

    // STR
    static result_code serialize_t_str(AGEA_serialization_args);
    static result_code deserialize_t_str(AGEA_deserialization_args);

    // STR
    static result_code serialize_t_id(AGEA_serialization_args);
    static result_code deserialize_t_id(AGEA_deserialization_args);

    // Bool
    static result_code serialize_t_bool(AGEA_serialization_args);
    static result_code deserialize_t_bool(AGEA_deserialization_args);

    // I8
    static result_code serialize_t_i8(AGEA_serialization_args);
    static result_code deserialize_t_i8(AGEA_deserialization_args);

    // I16
    static result_code serialize_t_i16(AGEA_serialization_args);
    static result_code deserialize_t_i16(AGEA_deserialization_args);

    // I32
    static result_code serialize_t_i32(AGEA_serialization_args);
    static result_code deserialize_t_i32(AGEA_deserialization_args);

    // I64
    static result_code serialize_t_i64(AGEA_serialization_args);
    static result_code deserialize_t_i64(AGEA_deserialization_args);

    // U8
    static result_code serialize_t_u8(AGEA_serialization_args);
    static result_code deserialize_t_u8(AGEA_deserialization_args);

    // U16
    static result_code serialize_t_u16(AGEA_serialization_args);
    static result_code deserialize_t_u16(AGEA_deserialization_args);

    // U32
    static result_code serialize_t_u32(AGEA_serialization_args);
    static result_code deserialize_t_u32(AGEA_deserialization_args);

    // U64
    static result_code serialize_t_u64(AGEA_serialization_args);
    static result_code deserialize_t_u64(AGEA_deserialization_args);

    // Float
    static result_code serialize_t_f(AGEA_serialization_args);
    static result_code deserialize_t_f(AGEA_deserialization_args);

    // Double
    static result_code serialize_t_d(AGEA_serialization_args);
    static result_code deserialize_t_d(AGEA_deserialization_args);

    // Vec3
    static result_code serialize_t_vec3(AGEA_serialization_args);
    static result_code deserialize_t_vec3(AGEA_deserialization_args);

    // Material
    static result_code serialize_t_txt(AGEA_serialization_args);
    static result_code deserialize_t_txt(AGEA_deserialization_args);

    // Material
    static result_code serialize_t_mat(AGEA_serialization_args);
    static result_code deserialize_t_mat(AGEA_deserialization_args);

    // Mesh
    static result_code serialize_t_msh(AGEA_serialization_args);
    static result_code deserialize_t_msh(AGEA_deserialization_args);

    // Obj
    static result_code serialize_t_obj(AGEA_serialization_args);
    static result_code deserialize_t_obj(AGEA_deserialization_args);

    // Se
    static result_code serialize_t_se(AGEA_serialization_args);
    static result_code deserialize_t_se(AGEA_deserialization_args);

    // Com
    static result_code serialize_t_com(AGEA_serialization_args);
    static result_code deserialize_t_com(AGEA_deserialization_args);

    // Color
    static result_code serialize_t_color(AGEA_serialization_args);
    static result_code deserialize_t_color(AGEA_deserialization_args);

    // Buffer
    static result_code serialize_t_buf(AGEA_serialization_args);
    static result_code deserialize_t_buf(AGEA_deserialization_args);
};

}  // namespace reflection
}  // namespace agea
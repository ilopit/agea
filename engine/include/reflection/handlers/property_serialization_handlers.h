#pragma once

#include "core/agea_minimal.h"

#include "serialization/serialization_fwds.h"

#include "reflection/property_utils.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{

struct property_serialization_handlers
{
    static std::vector<reflection::property_serialization_handler>&
    serializers()
    {
        static std::vector<reflection::property_serialization_handler> s_serializers;
        return s_serializers;
    }

    static std::vector<reflection::property_serialization_handler>&
    deserializers()
    {
        static std::vector<reflection::property_serialization_handler> s_deserializers;
        return s_deserializers;
    }

    static bool
    init();

    // STR
    static bool serialize_t_str(AGEA_deseialization_args);
    static bool deserialize_t_str(AGEA_deseialization_args);

    // Bool
    static bool serialize_t_bool(AGEA_deseialization_args);
    static bool deserialize_t_bool(AGEA_deseialization_args);

    // I8
    static bool serialize_t_i8(AGEA_deseialization_args);
    static bool deserialize_t_i8(AGEA_deseialization_args);

    // I16
    static bool serialize_t_i16(AGEA_deseialization_args);
    static bool deserialize_t_i16(AGEA_deseialization_args);

    // I32
    static bool serialize_t_i32(AGEA_deseialization_args);
    static bool deserialize_t_i32(AGEA_deseialization_args);

    // I64
    static bool serialize_t_i64(AGEA_deseialization_args);
    static bool deserialize_t_i64(AGEA_deseialization_args);

    // U8
    static bool serialize_t_u8(AGEA_deseialization_args);
    static bool deserialize_t_u8(AGEA_deseialization_args);

    // U16
    static bool serialize_t_u16(AGEA_deseialization_args);
    static bool deserialize_t_u16(AGEA_deseialization_args);

    // U32
    static bool serialize_t_u32(AGEA_deseialization_args);
    static bool deserialize_t_u32(AGEA_deseialization_args);

    // U64
    static bool serialize_t_u64(AGEA_deseialization_args);
    static bool deserialize_t_u64(AGEA_deseialization_args);

    // Float
    static bool serialize_t_f(AGEA_deseialization_args);
    static bool deserialize_t_f(AGEA_deseialization_args);

    // Double
    static bool serialize_t_d(AGEA_deseialization_args);
    static bool deserialize_t_d(AGEA_deseialization_args);

    // Vec3
    static bool serialize_t_vec3(AGEA_deseialization_args);
    static bool deserialize_t_vec3(AGEA_deseialization_args);

    // Material
    static bool serialize_t_txt(AGEA_deseialization_args);
    static bool deserialize_t_txt(AGEA_deseialization_args);

    // Material
    static bool serialize_t_mat(AGEA_deseialization_args);
    static bool deserialize_t_mat(AGEA_deseialization_args);

    // Mesh
    static bool serialize_t_msh(AGEA_deseialization_args);
    static bool deserialize_t_msh(AGEA_deseialization_args);

    // Obj
    static bool serialize_t_obj(AGEA_deseialization_args);
    static bool deserialize_t_obj(AGEA_deseialization_args);

    // Com
    static bool serialize_t_com(AGEA_deseialization_args);
    static bool deserialize_t_com(AGEA_deseialization_args);
};

}  // namespace reflection
}  // namespace agea
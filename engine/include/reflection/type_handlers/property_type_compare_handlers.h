#pragma once

#include "core/agea_minimal.h"

#include "reflection/property_utils.h"

#include "model/model_fwds.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{
struct property_type_compare_handlers
{
    static std::vector<type_compare_handler>&
    compare_handlers()
    {
        static std::vector<type_compare_handler> s_copyrs;
        return s_copyrs;
    }

    static bool
    init();

    // STR
    static bool compare_t_str(AGEA_compare_handler_args);

    // Bool
    static bool compare_t_bool(AGEA_compare_handler_args);

    // I8
    static bool compare_t_i8(AGEA_compare_handler_args);

    // I16
    static bool compare_t_i16(AGEA_compare_handler_args);

    // I32
    static bool compare_t_i32(AGEA_compare_handler_args);

    // I64
    static bool compare_t_i64(AGEA_compare_handler_args);

    // U8
    static bool compare_t_u8(AGEA_compare_handler_args);

    // U16
    static bool compare_t_u16(AGEA_compare_handler_args);

    // U32
    static bool compare_t_u32(AGEA_compare_handler_args);

    // U64
    static bool compare_t_u64(AGEA_compare_handler_args);

    // Float
    static bool compare_t_f(AGEA_compare_handler_args);

    // Double
    static bool compare_t_d(AGEA_compare_handler_args);

    // Vec3
    static bool compare_t_vec3(AGEA_compare_handler_args);

    // Material
    static bool compare_t_txt(AGEA_compare_handler_args);

    // Material
    static bool compare_t_mat(AGEA_compare_handler_args);

    // Mesh
    static bool compare_t_msh(AGEA_compare_handler_args);

    // Obj
    static bool compare_t_obj(AGEA_compare_handler_args);

    // Com
    static bool compare_t_com(AGEA_compare_handler_args);
};

}  // namespace reflection
}  // namespace agea
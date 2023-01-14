#pragma once

#include "model/model_minimal.h"

#include "model/reflection/property_utils.h"

#include "model/model_fwds.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{
struct property_type_copy_handlers
{
    static std::vector<type_copy_handler>&
    copy_handlers()
    {
        static std::vector<type_copy_handler> s_copyrs;
        return s_copyrs;
    }

    static bool
    init();

    // STR
    static bool copy_t_str(AGEA_copy_handler_args);

    // STR
    static bool copy_t_id(AGEA_copy_handler_args);

    // Bool
    static bool copy_t_bool(AGEA_copy_handler_args);

    // I8
    static bool copy_t_i8(AGEA_copy_handler_args);

    // I16
    static bool copy_t_i16(AGEA_copy_handler_args);

    // I32
    static bool copy_t_i32(AGEA_copy_handler_args);

    // I64
    static bool copy_t_i64(AGEA_copy_handler_args);

    // U8
    static bool copy_t_u8(AGEA_copy_handler_args);

    // U16
    static bool copy_t_u16(AGEA_copy_handler_args);

    // U32
    static bool copy_t_u32(AGEA_copy_handler_args);

    // U64
    static bool copy_t_u64(AGEA_copy_handler_args);

    // Float
    static bool copy_t_f(AGEA_copy_handler_args);

    // Double
    static bool copy_t_d(AGEA_copy_handler_args);

    // Vec3
    static bool copy_t_vec3(AGEA_copy_handler_args);

    // Material
    static bool copy_t_txt(AGEA_copy_handler_args);

    // Material
    static bool copy_t_mat(AGEA_copy_handler_args);

    // Mesh
    static bool copy_t_msh(AGEA_copy_handler_args);

    // Obj
    static bool copy_t_obj(AGEA_copy_handler_args);

    // Com
    static bool copy_t_com(AGEA_copy_handler_args);

    // Com
    static bool copy_t_buf(AGEA_copy_handler_args);

    // Color
    static bool copy_t_col(AGEA_copy_handler_args);

    // Color
    static bool copy_t_se(AGEA_copy_handler_args);
};

}  // namespace reflection
}  // namespace agea
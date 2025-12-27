#pragma once

#include "core/reflection/property.h"
#include "core/reflection/reflection_type.h"

#include <functional>

namespace agea
{
namespace ui
{

class property_drawers
{
public:
    static void
    init();

    static result_code
    draw_ro(::agea::root::smart_object* obj, ::agea::reflection::property& p);

    // STR
    static result_code
    draw_ro_str(::agea::blob_ptr ptr);
    static result_code
    write_t_str(::agea::blob_ptr ptr);

    // Bool
    static result_code
    draw_ro_bool(::agea::blob_ptr ptr);
    static result_code
    write_t_bool(::agea::blob_ptr ptr);

    // I8
    static result_code
    draw_ro_i8(::agea::blob_ptr ptr);
    static result_code
    write_t_i8(::agea::blob_ptr ptr);

    // I16
    static result_code
    draw_ro_i16(::agea::blob_ptr ptr);
    static result_code
    write_t_i16(::agea::blob_ptr ptr);

    // I32
    static result_code
    draw_ro_i32(::agea::blob_ptr ptr);
    static result_code
    write_t_i32(::agea::blob_ptr ptr);

    // I64
    static result_code
    draw_ro_i64(::agea::blob_ptr ptr);
    static result_code
    write_t_i64(::agea::blob_ptr ptr);

    // U8
    static result_code
    draw_ro_u8(::agea::blob_ptr ptr);
    static result_code
    write_t_u8(::agea::blob_ptr ptr);

    // U16
    static result_code
    draw_ro_u16(::agea::blob_ptr ptr);
    static result_code
    write_t_u16(::agea::blob_ptr ptr);

    // U32
    static result_code
    draw_ro_u32(::agea::blob_ptr ptr);
    static result_code
    write_t_u32(::agea::blob_ptr ptr);

    // U64
    static result_code
    draw_ro_u64(::agea::blob_ptr ptr);
    static result_code
    write_t_u64(::agea::blob_ptr ptr);

    // Float
    static result_code
    draw_ro_f(::agea::blob_ptr ptr);
    static result_code
    write_t_f(::agea::blob_ptr ptr);

    // Double
    static result_code
    draw_ro_d(::agea::blob_ptr ptr);
    static result_code
    write_t_d(::agea::blob_ptr ptr);

    // Vec3
    static result_code
    draw_ro_vec3(::agea::blob_ptr ptr);
    static result_code
    write_t_vec3(::agea::blob_ptr ptr);

    // Material
    static result_code
    draw_ro_mat(::agea::blob_ptr ptr);
    static result_code
    write_t_mat(::agea::blob_ptr ptr);

    // Mesh
    static result_code
    draw_ro_msh(::agea::blob_ptr ptr);
    static result_code
    write_t_msh(::agea::blob_ptr ptr);

    static std::unordered_map<int, reflection::type_handler__to_string>&
    ro_drawers()
    {
        static std::unordered_map<int, reflection::type_handler__to_string> s_readers;
        return s_readers;
    }
};
}  // namespace ui
}  // namespace agea
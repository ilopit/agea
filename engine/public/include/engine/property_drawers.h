#pragma once

#include "core/reflection/property.h"
#include "core/reflection/reflection_type.h"

#include <functional>

namespace kryga
{
namespace ui
{

class property_drawers
{
public:
    static void
    init();

    static result_code
    draw_ro(::kryga::root::smart_object* obj, ::kryga::reflection::property& p);

    // STR
    static result_code
    draw_ro_str(::kryga::blob_ptr ptr);
    static result_code
    write_t_str(::kryga::blob_ptr ptr);

    // Bool
    static result_code
    draw_ro_bool(::kryga::blob_ptr ptr);
    static result_code
    write_t_bool(::kryga::blob_ptr ptr);

    // I8
    static result_code
    draw_ro_i8(::kryga::blob_ptr ptr);
    static result_code
    write_t_i8(::kryga::blob_ptr ptr);

    // I16
    static result_code
    draw_ro_i16(::kryga::blob_ptr ptr);
    static result_code
    write_t_i16(::kryga::blob_ptr ptr);

    // I32
    static result_code
    draw_ro_i32(::kryga::blob_ptr ptr);
    static result_code
    write_t_i32(::kryga::blob_ptr ptr);

    // I64
    static result_code
    draw_ro_i64(::kryga::blob_ptr ptr);
    static result_code
    write_t_i64(::kryga::blob_ptr ptr);

    // U8
    static result_code
    draw_ro_u8(::kryga::blob_ptr ptr);
    static result_code
    write_t_u8(::kryga::blob_ptr ptr);

    // U16
    static result_code
    draw_ro_u16(::kryga::blob_ptr ptr);
    static result_code
    write_t_u16(::kryga::blob_ptr ptr);

    // U32
    static result_code
    draw_ro_u32(::kryga::blob_ptr ptr);
    static result_code
    write_t_u32(::kryga::blob_ptr ptr);

    // U64
    static result_code
    draw_ro_u64(::kryga::blob_ptr ptr);
    static result_code
    write_t_u64(::kryga::blob_ptr ptr);

    // Float
    static result_code
    draw_ro_f(::kryga::blob_ptr ptr);
    static result_code
    write_t_f(::kryga::blob_ptr ptr);

    // Double
    static result_code
    draw_ro_d(::kryga::blob_ptr ptr);
    static result_code
    write_t_d(::kryga::blob_ptr ptr);

    // Vec3
    static result_code
    draw_ro_vec3(::kryga::blob_ptr ptr);
    static result_code
    write_t_vec3(::kryga::blob_ptr ptr);

    // Material
    static result_code
    draw_ro_mat(::kryga::blob_ptr ptr);
    static result_code
    write_t_mat(::kryga::blob_ptr ptr);

    // Mesh
    static result_code
    draw_ro_msh(::kryga::blob_ptr ptr);
    static result_code
    write_t_msh(::kryga::blob_ptr ptr);

    static std::unordered_map<int, reflection::type_handler__to_string>&
    ro_drawers()
    {
        static std::unordered_map<int, reflection::type_handler__to_string> s_readers;
        return s_readers;
    }
};
}  // namespace ui
}  // namespace kryga
#pragma once

#include "model/reflection/property.h"

#include <functional>

namespace agea
{

namespace model
{
class smart_object;
}  // namespace model

namespace ui
{

class property_drawers
{
public:
    static void
    init();

    using draw_ro_property = std::function<bool(::agea::blob_ptr)>;

    static bool
    draw_ro(::agea::model::smart_object* obj, ::agea::reflection::property& p);

    // STR
    static bool
    draw_ro_str(::agea::blob_ptr ptr);
    static bool
    write_t_str(::agea::blob_ptr ptr);

    // Bool
    static bool
    draw_ro_bool(::agea::blob_ptr ptr);
    static bool
    write_t_bool(::agea::blob_ptr ptr);

    // I8
    static bool
    draw_ro_i8(::agea::blob_ptr ptr);
    static bool
    write_t_i8(::agea::blob_ptr ptr);

    // I16
    static bool
    draw_ro_i16(::agea::blob_ptr ptr);
    static bool
    write_t_i16(::agea::blob_ptr ptr);

    // I32
    static bool
    draw_ro_i32(::agea::blob_ptr ptr);
    static bool
    write_t_i32(::agea::blob_ptr ptr);

    // I64
    static bool
    draw_ro_i64(::agea::blob_ptr ptr);
    static bool
    write_t_i64(::agea::blob_ptr ptr);

    // U8
    static bool
    draw_ro_u8(::agea::blob_ptr ptr);
    static bool
    write_t_u8(::agea::blob_ptr ptr);

    // U16
    static bool
    draw_ro_u16(::agea::blob_ptr ptr);
    static bool
    write_t_u16(::agea::blob_ptr ptr);

    // U32
    static bool
    draw_ro_u32(::agea::blob_ptr ptr);
    static bool
    write_t_u32(::agea::blob_ptr ptr);

    // U64
    static bool
    draw_ro_u64(::agea::blob_ptr ptr);
    static bool
    write_t_u64(::agea::blob_ptr ptr);

    // Float
    static bool
    draw_ro_f(::agea::blob_ptr ptr);
    static bool
    write_t_f(::agea::blob_ptr ptr);

    // Double
    static bool
    draw_ro_d(::agea::blob_ptr ptr);
    static bool
    write_t_d(::agea::blob_ptr ptr);

    // Vec3
    static bool
    draw_ro_vec3(::agea::blob_ptr ptr);
    static bool
    write_t_vec3(::agea::blob_ptr ptr);

    // Material
    static bool
    draw_ro_mat(::agea::blob_ptr ptr);
    static bool
    write_t_mat(::agea::blob_ptr ptr);

    // Mesh
    static bool
    draw_ro_msh(::agea::blob_ptr ptr);
    static bool
    write_t_msh(::agea::blob_ptr ptr);

    static std::vector<draw_ro_property>&
    ro_drawers()
    {
        static std::vector<draw_ro_property> s_readers;
        return s_readers;
    }
};
}  // namespace ui
}  // namespace agea
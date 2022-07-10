#include "model/reflection/type_handlers/property_type_compare_handlers.h"

#include "model/level.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/materials_cache.h"

#include "model/object_constructor.h"

namespace agea
{
namespace reflection
{
namespace
{

}  // namespace

bool
property_type_compare_handlers::init()
{
    using namespace reflection;
    // clang-format off
    compare_handlers()  .resize((size_t)property_type::t_last, nullptr);

    compare_handlers()  [(size_t)property_type::t_str]  = property_type_compare_handlers::compare_t_str;
    compare_handlers()  [(size_t)property_type::t_bool] = property_type_compare_handlers::compare_t_bool;
    compare_handlers()  [(size_t)property_type::t_i8]   = property_type_compare_handlers::compare_t_i8;
    compare_handlers()  [(size_t)property_type::t_i16]  = property_type_compare_handlers::compare_t_i16;
    compare_handlers()  [(size_t)property_type::t_i32]  = property_type_compare_handlers::compare_t_i32;
    compare_handlers()  [(size_t)property_type::t_i64]  = property_type_compare_handlers::compare_t_i64;
    compare_handlers()  [(size_t)property_type::t_u8]   = property_type_compare_handlers::compare_t_u8;
    compare_handlers()  [(size_t)property_type::t_u16]  = property_type_compare_handlers::compare_t_u16;
    compare_handlers()  [(size_t)property_type::t_u32]  = property_type_compare_handlers::compare_t_u32;
    compare_handlers()  [(size_t)property_type::t_u64]  = property_type_compare_handlers::compare_t_u64;
    compare_handlers()  [(size_t)property_type::t_f]    = property_type_compare_handlers::compare_t_f;
    compare_handlers()  [(size_t)property_type::t_d]    = property_type_compare_handlers::compare_t_d;
    compare_handlers()  [(size_t)property_type::t_vec3] = property_type_compare_handlers::compare_t_vec3;
    compare_handlers()  [(size_t)property_type::t_txt]  = property_type_compare_handlers::compare_t_txt;
    compare_handlers()  [(size_t)property_type::t_mat]  = property_type_compare_handlers::compare_t_mat;
    compare_handlers()  [(size_t)property_type::t_msh]  = property_type_compare_handlers::compare_t_msh;
    compare_handlers()  [(size_t)property_type::t_obj]  = property_type_compare_handlers::compare_t_obj;
    compare_handlers()  [(size_t)property_type::t_com]  = property_type_compare_handlers::compare_t_com;
    // clang-format on

    return true;
}

// STR
bool
property_type_compare_handlers::compare_t_str(AGEA_compare_handler_args)
{
    return full_compare<std::string>(from, to);
}

// Bool
bool
property_type_compare_handlers::compare_t_bool(AGEA_compare_handler_args)
{
    return fast_compare<bool>(from, to);
}

// I8
bool
property_type_compare_handlers::compare_t_i8(AGEA_compare_handler_args)
{
    return fast_compare<int8_t>(from, to);
}

// I16
bool
property_type_compare_handlers::compare_t_i16(AGEA_compare_handler_args)
{
    return fast_compare<int16_t>(from, to);
}

// I32
bool
property_type_compare_handlers::compare_t_i32(AGEA_compare_handler_args)
{
    return fast_compare<int32_t>(from, to);
}

// I64
bool
property_type_compare_handlers::compare_t_i64(AGEA_compare_handler_args)
{
    return fast_compare<int64_t>(from, to);
}
// U8
bool
property_type_compare_handlers::compare_t_u8(AGEA_compare_handler_args)
{
    return fast_compare<uint8_t>(from, to);
}

// U16
bool
property_type_compare_handlers::compare_t_u16(AGEA_compare_handler_args)
{
    return fast_compare<uint16_t>(from, to);
}

// U32
bool
property_type_compare_handlers::compare_t_u32(AGEA_compare_handler_args)
{
    return fast_compare<uint32_t>(from, to);
}

// U64
bool
property_type_compare_handlers::compare_t_u64(AGEA_compare_handler_args)
{
    return fast_compare<uint64_t>(from, to);
}

// Float
bool
property_type_compare_handlers::compare_t_f(AGEA_compare_handler_args)
{
    return fast_compare<float>(from, to);
}

// Double
bool
property_type_compare_handlers::compare_t_d(AGEA_compare_handler_args)
{
    return fast_compare<double>(from, to);
}

// Vec3
bool
property_type_compare_handlers::compare_t_vec3(AGEA_compare_handler_args)
{
    return full_compare<glm::vec3>(from, to);
}

// Texture
bool
property_type_compare_handlers::compare_t_txt(AGEA_compare_handler_args)
{
    return fast_compare<model::smart_object*>(from, to);
}

// Material
bool
property_type_compare_handlers::compare_t_mat(AGEA_compare_handler_args)
{
    return fast_compare<model::smart_object*>(from, to);
}

// Mesh
bool
property_type_compare_handlers::compare_t_msh(AGEA_compare_handler_args)
{
    return fast_compare<model::smart_object*>(from, to);
}

bool
property_type_compare_handlers::compare_t_obj(AGEA_compare_handler_args)
{
    return fast_compare<model::smart_object*>(from, to);
}

bool
property_type_compare_handlers::compare_t_com(AGEA_compare_handler_args)
{
    return fast_compare<model::smart_object*>(from, to);
}

}  // namespace reflection
}  // namespace agea
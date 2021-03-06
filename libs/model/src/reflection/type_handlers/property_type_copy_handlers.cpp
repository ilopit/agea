#include "model/reflection/type_handlers/property_type_copy_handlers.h"

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
property_type_copy_handlers::init()
{
    using namespace reflection;
    // clang-format off
    copy_handlers()  .resize((size_t)property_type::t_last, nullptr);

    copy_handlers()  [(size_t)property_type::t_str]  = property_type_copy_handlers::copy_t_str;
    copy_handlers()  [(size_t)property_type::t_id]   = property_type_copy_handlers::copy_t_id;
    copy_handlers()  [(size_t)property_type::t_bool] = property_type_copy_handlers::copy_t_bool;
    copy_handlers()  [(size_t)property_type::t_i8]   = property_type_copy_handlers::copy_t_i8;
    copy_handlers()  [(size_t)property_type::t_i16]  = property_type_copy_handlers::copy_t_i16;
    copy_handlers()  [(size_t)property_type::t_i32]  = property_type_copy_handlers::copy_t_i32;
    copy_handlers()  [(size_t)property_type::t_i64]  = property_type_copy_handlers::copy_t_i64;
    copy_handlers()  [(size_t)property_type::t_u8]   = property_type_copy_handlers::copy_t_u8;
    copy_handlers()  [(size_t)property_type::t_u16]  = property_type_copy_handlers::copy_t_u16;
    copy_handlers()  [(size_t)property_type::t_u32]  = property_type_copy_handlers::copy_t_u32;
    copy_handlers()  [(size_t)property_type::t_u64]  = property_type_copy_handlers::copy_t_u64;
    copy_handlers()  [(size_t)property_type::t_f]    = property_type_copy_handlers::copy_t_f;
    copy_handlers()  [(size_t)property_type::t_d]    = property_type_copy_handlers::copy_t_d;
    copy_handlers()  [(size_t)property_type::t_vec3] = property_type_copy_handlers::copy_t_vec3;
    copy_handlers()  [(size_t)property_type::t_txt]  = property_type_copy_handlers::copy_t_txt;
    copy_handlers()  [(size_t)property_type::t_mat]  = property_type_copy_handlers::copy_t_mat;
    copy_handlers()  [(size_t)property_type::t_msh]  = property_type_copy_handlers::copy_t_msh;
    copy_handlers()  [(size_t)property_type::t_obj]  = property_type_copy_handlers::copy_t_obj;
    copy_handlers()  [(size_t)property_type::t_com]  = property_type_copy_handlers::copy_t_com;
    // clang-format on

    return true;
}

// STR
bool
property_type_copy_handlers::copy_t_str(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    full_copy<std::string>(from, to);
    return true;
}

// STR
bool
property_type_copy_handlers::copy_t_id(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    full_copy<utils::id>(from, to);
    return true;
}

// Bool
bool
property_type_copy_handlers::copy_t_bool(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<bool>(from, to);
    return true;
}

// I8
bool
property_type_copy_handlers::copy_t_i8(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<int8_t>(from, to);
    return true;
}

// I16
bool
property_type_copy_handlers::copy_t_i16(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<int16_t>(from, to);
    return true;
}

// I32
bool
property_type_copy_handlers::copy_t_i32(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<int32_t>(from, to);
    return true;
}

// I64
bool
property_type_copy_handlers::copy_t_i64(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<int64_t>(from, to);
    return true;
}
// U8
bool
property_type_copy_handlers::copy_t_u8(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<uint8_t>(from, to);
    return true;
}

// U16
bool
property_type_copy_handlers::copy_t_u16(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<uint16_t>(from, to);
    return true;
}

// U32
bool
property_type_copy_handlers::copy_t_u32(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<uint32_t>(from, to);
    return true;
}

// U64
bool
property_type_copy_handlers::copy_t_u64(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<uint64_t>(from, to);
    return true;
}

// Float
bool
property_type_copy_handlers::copy_t_f(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<float>(from, to);
    return true;
}

// Double
bool
property_type_copy_handlers::copy_t_d(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<double>(from, to);
    return true;
}

// Vec3
bool
property_type_copy_handlers::copy_t_vec3(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    full_copy<glm::vec3>(from, to);
    return true;
}

// Texture
bool
property_type_copy_handlers::copy_t_txt(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<model::smart_object*>(from, to);
    return true;
}

// Material
bool
property_type_copy_handlers::copy_t_mat(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<model::smart_object*>(from, to);
    return true;
}

// Mesh
bool
property_type_copy_handlers::copy_t_msh(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    fast_copy<model::smart_object*>(from, to);
    return true;
}

bool
property_type_copy_handlers::copy_t_obj(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    AGEA_unused(from);
    AGEA_unused(to);

    AGEA_never("We should never be here!");
    return true;
}

bool
property_type_copy_handlers::copy_t_com(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    auto& f = extract<::agea::model::component*>(from);
    auto& t = extract<::agea::model::component*>(to);

    auto new_id =
        utils::id::from(dst_obj.get_id().str() + "/" + f->get_class_obj()->get_id().str());

    auto p = model::object_constructor::object_clone_create(*f, new_id, ooc);

    t = (::agea::model::component*)p;

    return true;
}

}  // namespace reflection
}  // namespace agea
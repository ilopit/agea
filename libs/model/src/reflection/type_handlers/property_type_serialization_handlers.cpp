#include "model/reflection/type_handlers/property_type_serialization_handlers.h"

#include "model/level.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/objects_cache.h"
#include "model/object_construction_context.h"
#include "model/caches/game_objects_cache.h"
#include "model/object_constructor.h"

#include "serialization/serialization.h"

#include "utils/id.h"

#include "utils/agea_log.h"

namespace agea
{
namespace reflection
{

bool
property_type_serialization_handlers::init()
{
    using namespace reflection;
    // clang-format off
    serializers()  .resize((size_t)property_type::t_last, nullptr);
    deserializers().resize((size_t)property_type::t_last, nullptr);

    serializers()  [(size_t)property_type::t_str]  = serialize_t_str;
    deserializers()[(size_t)property_type::t_str]  = deserialize_t_str;

    serializers()  [(size_t)property_type::t_id]   = serialize_t_id;
    deserializers()[(size_t)property_type::t_id]   = deserialize_t_id;

    serializers()  [(size_t)property_type::t_bool] = serialize_t_bool;
    deserializers()[(size_t)property_type::t_bool] = deserialize_t_bool;

    serializers()  [(size_t)property_type::t_i8]   = serialize_t_i8;
    deserializers()[(size_t)property_type::t_i8]   = deserialize_t_i8;

    serializers()  [(size_t)property_type::t_i16]  = serialize_t_i16;
    deserializers()[(size_t)property_type::t_i16]  = deserialize_t_i16;

    serializers()  [(size_t)property_type::t_i32]  = serialize_t_i32;
    deserializers()[(size_t)property_type::t_i32]  = deserialize_t_i32;

    serializers()  [(size_t)property_type::t_i64]  = serialize_t_i64;
    deserializers()[(size_t)property_type::t_i64]  = deserialize_t_i64;

    serializers()  [(size_t)property_type::t_u8]   = serialize_t_u8;
    deserializers()[(size_t)property_type::t_u8]   = deserialize_t_u8;

    serializers()  [(size_t)property_type::t_u16]  = serialize_t_u16;
    deserializers()[(size_t)property_type::t_u16]  = deserialize_t_u16;

    serializers()  [(size_t)property_type::t_u32]  = serialize_t_u32;
    deserializers()[(size_t)property_type::t_u32]  = deserialize_t_u32;

    serializers()  [(size_t)property_type::t_u64]  = serialize_t_u64;
    deserializers()[(size_t)property_type::t_u64]  = deserialize_t_u64;

    serializers()  [(size_t)property_type::t_f]    = serialize_t_f;
    deserializers()[(size_t)property_type::t_f]    = deserialize_t_f;

    serializers()  [(size_t)property_type::t_d]    = serialize_t_d;
    deserializers()[(size_t)property_type::t_d]    = deserialize_t_d;

    serializers()  [(size_t)property_type::t_vec3] = serialize_t_vec3;
    deserializers()[(size_t)property_type::t_vec3] = deserialize_t_vec3;

    serializers()  [(size_t)property_type::t_txt]  = serialize_t_txt;
    deserializers()[(size_t)property_type::t_txt]  = deserialize_t_txt;

    serializers()  [(size_t)property_type::t_mat]  = serialize_t_mat;
    deserializers()[(size_t)property_type::t_mat]  = deserialize_t_mat;

    serializers()  [(size_t)property_type::t_msh]  = serialize_t_msh;
    deserializers()[(size_t)property_type::t_msh]  = deserialize_t_msh;

    serializers()  [(size_t)property_type::t_obj]  = serialize_t_obj;
    deserializers()[(size_t)property_type::t_obj]  = deserialize_t_obj;

    serializers()  [(size_t)property_type::t_com]  = serialize_t_com;
    deserializers()[(size_t)property_type::t_com]  = deserialize_t_com;
    // clang-format on

    return true;
}

// STR
bool
property_type_serialization_handlers::serialize_t_str(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::pack_field<std::string>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_str(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::extract_field<std::string>(ptr, jc);
    return true;
}

// ID
bool
property_type_serialization_handlers::serialize_t_id(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    jc = extract<utils::id>(ptr).str();
    return true;
}

bool
property_type_serialization_handlers::deserialize_t_id(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    extract<utils::id>(ptr) = utils::id::from(jc.as<std::string>());
    return true;
}

// Bool
bool
property_type_serialization_handlers::serialize_t_bool(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<bool>(ptr, jc);

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_bool(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<bool>(ptr, jc);
    return true;
}

// I8
bool
property_type_serialization_handlers::serialize_t_i8(AGEA_serialization_args)
{
    AGEA_unused(ptr);

    reflection::pack_field<std::int8_t>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_i8(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::int8_t>(ptr) = jc.as<int8_t>();
    return true;
}

// I16
bool
property_type_serialization_handlers::serialize_t_i16(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::pack_field<std::int16_t>(ptr, jc);

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_i16(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::int16_t>(ptr) = jc.as<std::int16_t>();
    return true;
}

// I32
bool
property_type_serialization_handlers::serialize_t_i32(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::int32_t>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_i32(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int32_t>(ptr, jc);
    return true;
}

// I64
bool
property_type_serialization_handlers::serialize_t_i64(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::int64_t>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_i64(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::int64_t>(ptr, jc);
    return true;
}

// U8
bool
property_type_serialization_handlers::serialize_t_u8(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    reflection::pack_field<std::uint8_t>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_u8(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::uint8_t>(ptr) = jc.as<std::uint8_t>();
    return true;
}

// U16
bool
property_type_serialization_handlers::serialize_t_u16(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::uint16_t>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_u16(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract<std::uint16_t>(ptr) = jc.as<std::uint16_t>();
    return true;
}

// U32
bool
property_type_serialization_handlers::serialize_t_u32(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::uint32_t>(ptr, jc);

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_u32(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::uint32_t>(ptr, jc);
    return true;
}

// U64
bool
property_type_serialization_handlers::serialize_t_u64(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<std::uint64_t>(ptr, jc);

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_u64(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<std::uint64_t>(ptr, jc);
    return true;
}

// Float
bool
property_type_serialization_handlers::serialize_t_f(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<float>(ptr, jc);

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_f(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<float>(ptr, jc);
    return true;
}

// Double
bool
property_type_serialization_handlers::serialize_t_d(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    reflection::pack_field<double>(ptr, jc);

    return true;
}
bool
property_type_serialization_handlers::deserialize_t_d(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    reflection::extract_field<double>(ptr, jc);
    return true;
}

// Vec3
bool
property_type_serialization_handlers::serialize_t_vec3(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::extract<glm::vec3>(ptr);

    jc["x"] = field.x;
    jc["y"] = field.y;
    jc["z"] = field.z;

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_vec3(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<glm::vec3>(ptr);

    field.x = jc["x"].as<float>();
    field.y = jc["y"].as<float>();
    field.z = jc["z"].as<float>();

    return true;
}

// Texture
bool
property_type_serialization_handlers::serialize_t_txt(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::texture*>(ptr);
    jc = field->get_id().str();

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_txt(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<::agea::model::texture*>(ptr);

    const auto& txt_id = utils::id::from(jc.as<std::string>());

    auto txt = occ.m_class_local_set.textures->get_item(txt_id);

    if (!txt)
    {
        ALOG_INFO("Failed to find [{0}] in local cache, fallback to global", txt_id.str());
        txt = occ.m_class_global_set.textures->get_item(txt_id);
        if (!txt)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }
    field = txt;
    return true;
}

// Material
bool
property_type_serialization_handlers::serialize_t_mat(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::material*>(ptr);
    jc = field->get_id().str();

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_mat(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<::agea::model::material*>(ptr);
    const auto& mat_id = utils::id::from(jc.as<std::string>());

    auto mat = occ.m_class_local_set.materials->get_item(mat_id);

    if (!mat)
    {
        ALOG_INFO("Failed to find [{0}] in local cache, fallback to global", mat_id.str());
        mat = occ.m_class_local_set.materials->get_item(mat_id);
        if (!mat)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }
    field = mat;
    return true;
}

// Mesh
bool
property_type_serialization_handlers::serialize_t_msh(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::mesh*>(ptr);
    jc = field->get_id().str();

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_msh(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::extract<::agea::model::mesh*>(ptr);
    const auto& mesh_id = utils::id::from(jc.as<std::string>());

    auto msh = occ.m_class_local_set.meshes->get_item(mesh_id);

    if (!msh)
    {
        ALOG_INFO("Failed to find [{0}] in local cache, fallback to global", mesh_id.str());
        msh = occ.m_class_global_set.meshes->get_item(mesh_id);
        if (!msh)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }
    field = msh;
    return true;
}

bool
property_type_serialization_handlers::serialize_t_obj(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);

    jc["id"] = field->get_id().str();

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_obj(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);

    auto id = utils::id::from(jc["id"].as<std::string>());

    auto pstr = glob::class_objects_cache::get()->get_item(id);

    field = pstr;

    return true;
}

bool
property_type_serialization_handlers::serialize_t_com(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::extract<::agea::model::smart_object*>(ptr);

    jc["id"] = field->get_id().str();
    jc["object_class"] = field->get_class_obj()->get_id().str();

    return true;
}

bool
property_type_serialization_handlers::deserialize_t_com(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    AGEA_not_implemented;
    //     auto& field = reflection::extract<::agea::model::smart_object*>(ptr);
    //
    //     auto class_id = core::id::from(jc["object_class"].as<std::string>());
    //
    //     auto id = core::id::from(jc["id"].as<std::string>());
    //
    //     auto obj = model::object_constructor::object_clone_create(class_id, id, occ);
    //
    //     if (!obj)
    //     {
    //         ALOG_ERROR("object [{0}] doesn't exists", class_id.str());
    //         return false;
    //     }
    //
    //     if (!model::object_constructor::update_object_properties(*obj, jc, occ))
    //     {
    //         ALOG_ERROR("object [{0}] update failed", class_id.str());
    //         return false;
    //     }
    //
    //     field = obj;

    return true;
}

}  // namespace reflection
}  // namespace agea
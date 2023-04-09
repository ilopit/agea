#include "model/model_module.h"

#include "model/reflection/reflection_type.h"
#include "model/reflection/property_utils.h"

#include "model/assets/asset.h"
#include "model/assets/material.h"
#include "model/assets/mesh.h"
#include "model/assets/texture.h"
#include "model/assets/shader_effect.h"

#include "model/components/component.h"
#include "model/caches/cache_set.h"
#include "model/caches/objects_cache.h"
#include "model/caches/materials_cache.h"

#include "model/object_load_context.h"
#include "model/object_constructor.h"
#include "model/game_object.h"
#include "model/package.h"

#include "model/reflection/reflection_type_utils.h"

#include "model/model_types_resolvers.ar.h"

#include <utils/agea_log.h>
#include <utils/string_utility.h>

#include <serialization/serialization.h>

namespace agea
{
namespace model
{

namespace
{

// ========  UTILS  ====================================

#define MAKE_POD_TYPE(id, type_name)                                   \
    {                                                                  \
        auto rt = glob::reflection_type_registry::getr().get_type(id); \
        rt->deserialization = default_deserialize<type_name>;          \
        rt->compare = default_compare<type_name>;                      \
        rt->copy = default_copy<type_name>;                            \
        rt->serialization = default_serialize<type_name>;              \
    }

template <typename T>
result_code
default_copy(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::as_type<T>(to) = reflection::utils::as_type<T>(from);
    return result_code::ok;
}

template <typename T>
result_code
default_serialize(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::utils::pack_field<T>(ptr, jc);

    return result_code::ok;
}

template <typename T>
result_code
default_deserialize(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::utils::extract_field<T>(ptr, jc);
    return result_code::ok;
}

template <typename T>
result_code
default_compare(AGEA_compare_handler_args)
{
    return reflection::utils::default_compare<T>(from, to);
}

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  model::object_load_context& occ,
                  model::architype a_type)
{
    auto& field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);

    const auto id = AID(jc.as<std::string>());

    return model::object_constructor::object_load_internal(id, occ, field);
}

result_code
copy_smart_object(AGEA_copy_handler_args)
{
    auto& s = src_obj;
    auto type = ooc.get_construction_type();
    if (type != model::object_load_type::class_obj)
    {
        auto& obj = reflection::utils::as_type<::agea::model::smart_object*>(from);
        auto& dst_obj = reflection::utils::as_type<::agea::model::smart_object*>(to);
        return model::object_constructor::object_clone_create_internal(obj->get_id(), obj->get_id(),
                                                                       ooc, dst_obj);
    }
    else
    {
        reflection::utils::default_copy<model::smart_object*>(from, to);
    }

    return result_code::ok;
}

// ========  ID  ====================================
result_code
serialize_t_id(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    jc = reflection::utils::as_type<utils::id>(ptr).str();
    return result_code::ok;
}

result_code
deserialize_t_id(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::utils::as_type<utils::id>(ptr) = AID(jc.as<std::string>());
    return result_code::ok;
}

// ========  VEC3  ====================================
result_code
serialize_t_vec3(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::utils::as_type<model::vec3>(ptr);

    jc["x"] = field.x;
    jc["y"] = field.y;
    jc["z"] = field.z;

    return result_code::ok;
}

result_code
deserialize_t_vec3(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<model::vec3>(ptr);

    field.x = jc["x"].as<float>();
    field.y = jc["y"].as<float>();
    field.z = jc["z"].as<float>();

    return result_code::ok;
}

// ========  TEXTURE  ====================================
result_code
serialize_t_txt(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::model::texture*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_txt(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::texture);
}

// ========  MATERIAL  ====================================
result_code
serialize_t_mat(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::model::material*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_mat(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::material);
}

result_code
copy_t_mat(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::default_copy<model::smart_object*>(from, to);
    return result_code::ok;
}

// ========  MESH  ====================================
result_code
serialize_t_msh(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::model::mesh*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_msh(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::mesh);
}

result_code
copy_t_msh(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::default_copy<model::smart_object*>(from, to);
    return result_code::ok;
}

// ========  OBJ  ====================================
result_code
serialize_t_obj(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);

    jc["id"] = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_obj(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);

    auto id = AID(jc["id"].as<std::string>());

    //     auto pstr = ::agea::glob::class_objects_cache::get()->get_item(id);
    //
    //     field = pstr;

    return result_code::ok;
}

result_code
deserialize_from_proto_t_obj(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);

    ::agea::model::object_constructor::update_object_properties(*field, jc, occ);

    return result_code::ok;
}

result_code
copy_t_obj(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    AGEA_unused(from);
    AGEA_unused(to);

    AGEA_never("We should never be here!");
    return result_code::ok;
}

// ========  SHADER_EFFECT  ====================================
result_code
serialize_t_se(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_se(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::shader_effect);
}

result_code
copy_t_se(AGEA_copy_handler_args)
{
    return copy_smart_object(src_obj, dst_obj, from, to, ooc);
}  // namespace

// ========  COMPONENT  ====================================
result_code
serialize_t_com(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);

    jc["id"] = field->get_id().str();
    jc["object_class"] = field->get_class_obj()->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_com(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    AGEA_not_implemented;

    return result_code::ok;
}

result_code
deserialize_from_proto_t_com(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<::agea::model::smart_object*>(ptr);

    ::agea::model::object_constructor::update_object_properties(*field, jc, occ);

    return result_code::ok;
}

result_code
copy_t_com(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    auto& f = reflection::utils::as_type<::agea::model::smart_object*>(from);
    auto& t = reflection::utils::as_type<::agea::model::smart_object*>(to);

    auto new_id = AID(dst_obj.get_id().str() + "/" + f->get_class_obj()->get_id().str());

    auto p = model::object_constructor::object_clone_create_internal(*f, new_id, ooc, t);

    t = (::agea::model::component*)p;

    return result_code::ok;
}

// ========  COLOR  ====================================
result_code
serialize_t_color(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::utils::as_type<::agea::model::color>(ptr);

    return result_code::ok;
}

result_code
deserialize_t_color(AGEA_deserialization_args)
{
    auto str_color = jc.as<std::string>();

    if (str_color.size() != 0)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    uint8_t rgba[4] = {0, 0, 0, 255};

    agea::string_utils::convert_hext_string_to_bytes(8, str_color.data(), rgba);

    auto& field = reflection::utils::as_type<::agea::model::color>(ptr);

    field.m_data.r = rgba[0] ? (rgba[0] / 255.f) : 0.f;
    field.m_data.g = rgba[1] ? (rgba[1] / 255.f) : 0.f;
    field.m_data.b = rgba[2] ? (rgba[2] / 255.f) : 0.f;
    field.m_data.a = rgba[3] ? (rgba[3] / 255.f) : 0.f;

    return result_code::ok;
}

result_code
copy_t_color(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    reflection::utils::default_copy<::agea::model::color>(from, to);
    return result_code::ok;
}

result_code
compare_t_color(AGEA_compare_handler_args)
{
    return reflection::utils::default_compare<::agea::model::color>(from, to);
}

// ========  BUFFER  ====================================
result_code
serialize_t_buf(AGEA_serialization_args)
{
    auto& field = reflection::utils::as_type<::agea::utils::buffer>(ptr);

    auto package_path = obj.get_package()->get_relative_path(field.get_file());

    if (!utils::buffer::save(field))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    jc = package_path.str();

    return result_code::ok;
}

result_code
deserialize_t_buf(AGEA_deserialization_args)
{
    auto rel_path = APATH(jc.as<std::string>());

    utils::path package_path;

    if (!occ.make_full_path(rel_path, package_path))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto& f = reflection::utils::as_type<::agea::utils::buffer>(ptr);
    f.set_file(package_path);

    if (!utils::buffer::load(package_path, f))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return result_code::ok;
}

result_code
copy_t_buf(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    reflection::utils::default_copy<::agea::utils::buffer>(from, to);

    return result_code::ok;
}

}  // namespace

bool
model_module::override_reflection_types()
{
    auto model_id = AID("model");

    MAKE_POD_TYPE(agea::model::model__string, std::string);
    MAKE_POD_TYPE(agea::model::model__bool, bool);

    MAKE_POD_TYPE(agea::model::model__int8_t, std::int8_t);
    MAKE_POD_TYPE(agea::model::model__int16_t, std::int16_t);
    MAKE_POD_TYPE(agea::model::model__int32_t, std::int32_t);
    MAKE_POD_TYPE(agea::model::model__int64_t, std::int64_t);

    MAKE_POD_TYPE(agea::model::model__uint8_t, std::uint8_t);
    MAKE_POD_TYPE(agea::model::model__uint16_t, std::uint16_t);
    MAKE_POD_TYPE(agea::model::model__uint32_t, std::uint32_t);
    MAKE_POD_TYPE(agea::model::model__uint64_t, std::uint64_t);

    MAKE_POD_TYPE(agea::model::model__float, float);
    MAKE_POD_TYPE(agea::model::model__double, double);

    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__id);

        rt->deserialization = deserialize_t_id;
        rt->compare = default_compare<utils::id>;
        rt->copy = default_copy<utils::id>;
        rt->serialization = serialize_t_id;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__buffer);

        rt->deserialization = deserialize_t_buf;
        rt->copy = default_copy<utils::buffer>;
        rt->serialization = serialize_t_buf;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__vec3);

        rt->deserialization = deserialize_t_vec3;
        rt->compare = default_compare<glm::vec3>;
        rt->copy = default_copy<glm::vec3>;
        rt->serialization = serialize_t_vec3;
    }

    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__smart_object);

        rt->deserialization = deserialize_t_obj;
        rt->compare = default_compare<model::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_obj;
        rt->deserialization_with_proto = deserialize_from_proto_t_obj;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__component);

        rt->deserialization = deserialize_t_com;
        rt->compare = default_compare<model::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_com;
        rt->deserialization_with_proto = deserialize_from_proto_t_com;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__texture);

        rt->deserialization = deserialize_t_txt;
        rt->compare = default_compare<model::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_txt;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__material);

        rt->deserialization = deserialize_t_mat;
        rt->compare = default_compare<model::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_mat;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__mesh);

        rt->deserialization = deserialize_t_msh;
        rt->compare = default_compare<model::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_msh;
    }
    {
        auto rt =
            glob::reflection_type_registry::getr().get_type(agea::model::model__shader_effect);

        rt->deserialization = deserialize_t_se;
        rt->compare = default_compare<model::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_se;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__game_object);

        rt->deserialization = deserialize_t_se;
        rt->compare = default_compare<model::game_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_obj;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(
            agea::model::model__game_object_component);

        rt->deserialization = deserialize_t_com;
        rt->compare = default_compare<model::game_object_component*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_com;
        rt->deserialization_with_proto = deserialize_from_proto_t_com;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(agea::model::model__mesh_object);

        rt->deserialization = deserialize_t_se;
        rt->compare = default_compare<model::game_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_obj;
    }

    return true;
}

}  // namespace model
}  // namespace agea
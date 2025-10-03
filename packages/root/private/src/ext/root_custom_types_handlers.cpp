#include "packages/root/ext/root_custom_types_handlers.h"

#include "packages/root/package.root.h"

#include "packages/root/assets/mesh.h"
#include "packages/root/assets/material.h"
#include "packages/root/assets/texture.h"
#include "packages/root/assets/shader_effect.h"
#include "packages/root/assets/pbr_material.h"
#include "packages/root/assets/solid_color_material.h"
#include "packages/root/assets/simple_texture_material.h"
#include "packages/root/mesh_object.h"
#include "packages/root/lights/point_light.h"
#include "packages/root/lights/components/point_light_component.h"
#include "packages/root/components/mesh_component.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>

#include <utils/agea_log.h>
#include <utils/string_utility.h>

#include <serialization/serialization.h>
#include <utils/agea_log.h>
#include <utils/dynamic_object_builder.h>
#include <utils/static_initializer.h>

namespace agea::root::custom
{

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  core::object_load_context& occ,
                  core::architype a_type)
{
    auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    const auto id = AID(jc.as<std::string>());

    return core::object_constructor::object_load_internal(id, occ, field);
}

result_code
copy_smart_obj(AGEA_copy_handler_args)
{
    auto& s = src_obj;
    auto type = ooc.get_construction_type();
    if (type != core::object_load_type::class_obj)
    {
        auto& obj = reflection::utils::as_type<::agea::root::smart_object*>(from);
        auto& dst_obj = reflection::utils::as_type<::agea::root::smart_object*>(to);

        if (!obj)
        {
            return result_code::ok;
        }

        return core::object_constructor::object_clone_create_internal(obj->get_id(), obj->get_id(),
                                                                      ooc, dst_obj);
    }
    else
    {
        reflection::utils::default_copy<root::smart_object*>(from, to);
    }

    return result_code::ok;
}

agea::result_code
serialize_smart_obj(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    jc["id"] = field->get_id().str();

    return result_code::ok;
}

agea::result_code
deserialize_smart_obj(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    // auto id = AID(jc["id"].as<std::string>());

    //     auto pstr = ::agea::glob::class_objects_cache::get()->get_item(id);
    //
    //     field = pstr;

    return result_code::ok;
}

agea::result_code
deserialize_from_proto_smart_obj(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    ::agea::core::object_constructor::update_object_properties(*field, jc, occ);

    return result_code::ok;
}

agea::result_code
to_string_smart_obj(AGEA_reflection_type_ui_args)
{
    AGEA_unused(ptr);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    result = field ? field->get_id().str() : "empty";

    return result_code::ok;
}

agea::result_code
compare_smart_obj(AGEA_compare_handler_args)
{
    return result_code::failed;
}

agea::result_code
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

agea::result_code
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

agea::result_code
copy_t_buf(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    reflection::utils::default_copy<::agea::utils::buffer>(from, to);

    return result_code::ok;
}

agea::result_code
to_string_t_buf(AGEA_reflection_type_ui_args)
{
    auto& field = reflection::utils::as_type<::agea::utils::buffer>(ptr);

    result = field.get_file().str();

    return result_code::ok;
}

}  // namespace agea::root::custom

//
// // ========  ID  ====================================
agea::result_code
serialize_t_id(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    jc = agea::reflection::utils::as_type<agea::utils::id>(ptr).str();
    return agea::result_code::ok;
}

agea::result_code
deserialize_t_id(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    agea::reflection::utils::as_type<agea::utils::id>(ptr) = AID(jc.as<std::string>());
    return agea::result_code::ok;
}

agea::result_code
to_string_t_id(AGEA_reflection_type_ui_args)
{
    AGEA_unused(ptr);
    result = agea::reflection::utils::as_type<agea::utils::id>(ptr).str();
    return agea::result_code::ok;
}

// ========  VEC2  ====================================

agea::result_code
to_string_t_vec2(AGEA_reflection_type_ui_args)
{
    AGEA_unused(ptr);

    auto& field = agea::reflection::utils::as_type<::agea::root::vec3>(ptr);

    result = result = std::format("{} {}", field.x, field.y);

    return agea::result_code::ok;
}

// ========  VEC3  ====================================
agea::result_code
serialize_t_vec3(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = agea::reflection::utils::as_type<agea::root::vec3>(ptr);

    jc["x"] = field.x;
    jc["y"] = field.y;
    jc["z"] = field.z;

    return agea::result_code::ok;
}

agea::result_code
deserialize_t_vec3(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = agea::reflection::utils::as_type<agea::root::vec3>(ptr);

    field.x = jc["x"].as<float>();
    field.y = jc["y"].as<float>();
    field.z = jc["z"].as<float>();

    return agea::result_code::ok;
}

agea::result_code
to_string_t_vec3(AGEA_reflection_type_ui_args)
{
    AGEA_unused(ptr);

    auto& field = agea::reflection::utils::as_type<::agea::root::vec3>(ptr);

    result = result = std::format("{} {} {}", field.x, field.y, field.z);

    return agea::result_code::ok;
}

// ========  VEC4  ====================================

agea::result_code
to_string_t_vec4(AGEA_reflection_type_ui_args)
{
    AGEA_unused(ptr);

    auto& field = agea::reflection::utils::as_type<::agea::root::vec4>(ptr);

    result = result = std::format("{} {} {} {}", field.x, field.y, field.z, field.w);

    return agea::result_code::ok;
}

//
// // ========  STRING  ====================================
// result_code
// to_string_t_string(AGEA_reflection_type_ui_args)
// {
//     AGEA_unused(ptr);
//     result = reflection::utils::as_type<std::string>(ptr);
//     return result_code::ok;
// }
//
// // ========  TEXTURE  ====================================
// result_code
// serialize_t_txt(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto field = reflection::utils::as_type<::agea::root::texture*>(ptr);
//     jc = field->get_id().str();
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_txt(AGEA_deserialization_args)
// {
//     return load_smart_object(ptr, jc, occ, core::architype::texture);
// }
//
// // ========  MATERIAL  ====================================
// result_code
// serialize_t_mat(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto field = reflection::utils::as_type<::agea::root::material*>(ptr);
//     jc = field->get_id().str();
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_mat(AGEA_deserialization_args)
// {
//     return load_smart_object(ptr, jc, occ, core::architype::material);
// }
//
// result_code
// copy_t_mat(AGEA_copy_handler_args)
// {
//     AGEA_unused(dst_obj);
//     AGEA_unused(src_obj);
//     AGEA_unused(ooc);
//
//     reflection::utils::default_copy<root::smart_object*>(from, to);
//     return result_code::ok;
// }
//
// // ========  MESH  ====================================
// result_code
// serialize_t_msh(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto field = reflection::utils::as_type<::agea::root::mesh*>(ptr);
//     jc = field->get_id().str();
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_msh(AGEA_deserialization_args)
// {
//     return load_smart_object(ptr, jc, occ, core::architype::mesh);
// }
//
// result_code
// copy_t_msh(AGEA_copy_handler_args)
// {
//     AGEA_unused(dst_obj);
//     AGEA_unused(src_obj);
//     AGEA_unused(ooc);
//
//     reflection::utils::default_copy<root::smart_object*>(from, to);
//     return result_code::ok;
// }
//
// // ========  OBJ  ====================================
// result_code
// serialize_t_obj(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//
//     jc["id"] = field->get_id().str();
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_obj(AGEA_deserialization_args)
// {
//     AGEA_unused(occ);
//
//     auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//
//     auto id = AID(jc["id"].as<std::string>());
//
//     //     auto pstr = ::agea::glob::class_objects_cache::get()->get_item(id);
//     //
//     //     field = pstr;
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_from_proto_t_obj(AGEA_deserialization_update_args)
// {
//     AGEA_unused(occ);
//
//     auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//
//     ::agea::core::object_constructor::update_object_properties(*field, jc, occ);
//
//     return result_code::ok;
// }
//
//
// result_code
// to_string_t_obj(AGEA_reflection_type_ui_args)
// {
//     AGEA_unused(ptr);
//
//     auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//
//     result = field ? field->get_id().str() : "empty";
//
//     return result_code::ok;
// }
//
// // ========  SHADER_EFFECT  ====================================
// result_code
// serialize_t_se(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//     jc = field->get_id().str();
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_se(AGEA_deserialization_args)
// {
//     return load_smart_object(ptr, jc, occ, core::architype::shader_effect);
// }
//
// result_code
// copy_t_se(AGEA_copy_handler_args)
// {
//     return copy_smart_object(src_obj, dst_obj, from, to, ooc);
// }  // namespace
//
// // ========  COMPONENT  ====================================
// result_code
// serialize_t_com(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//
//     jc["id"] = field->get_id().str();
//     jc["object_class"] = field->get_class_obj()->get_id().str();
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_com(AGEA_deserialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//     AGEA_unused(occ);
//
//     AGEA_not_implemented;
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_from_proto_t_com(AGEA_deserialization_update_args)
// {
//     AGEA_unused(occ);
//
//     auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
//
//     ::agea::core::object_constructor::update_object_properties(*field, jc, occ);
//
//     return result_code::ok;
// }
//
// result_code
// copy_t_com(AGEA_copy_handler_args)
// {
//     AGEA_unused(src_obj);
//
//     auto& f = reflection::utils::as_type<::agea::root::smart_object*>(from);
//     auto& t = reflection::utils::as_type<::agea::root::smart_object*>(to);
//
//     auto new_id = AID(dst_obj.get_id().str() + "/" + f->get_class_obj()->get_id().str());
//
//     auto p = core::object_constructor::object_clone_create_internal(*f, new_id, ooc, t);
//
//     t = (::agea::root::component*)p;
//
//     return result_code::ok;
// }
//
// // ========  COLOR  ====================================
// result_code
// serialize_t_color(AGEA_serialization_args)
// {
//     AGEA_unused(ptr);
//     AGEA_unused(jc);
//
//     auto& field = reflection::utils::as_type<::agea::core::color>(ptr);
//
//     return result_code::ok;
// }
//
// result_code
// deserialize_t_color(AGEA_deserialization_args)
// {
//     auto str_color = jc.as<std::string>();
//
//     if (str_color.size() != 0)
//     {
//         ALOG_LAZY_ERROR;
//         return result_code::failed;
//     }
//
//     uint8_t rgba[4] = {0, 0, 0, 255};
//
//     agea::string_utils::convert_hext_string_to_bytes(8, str_color.data(), rgba);
//
//     auto& field = reflection::utils::as_type<::agea::core::color>(ptr);
//
//     field.m_data.r = rgba[0] ? (rgba[0] / 255.f) : 0.f;
//     field.m_data.g = rgba[1] ? (rgba[1] / 255.f) : 0.f;
//     field.m_data.b = rgba[2] ? (rgba[2] / 255.f) : 0.f;
//     field.m_data.a = rgba[3] ? (rgba[3] / 255.f) : 0.f;
//
//     return result_code::ok;
// }
//
// result_code
// copy_t_color(AGEA_copy_handler_args)
// {
//     AGEA_unused(src_obj);
//
//     reflection::utils::default_copy<::agea::core::color>(from, to);
//     return result_code::ok;
// }
//
// result_code
// compare_t_color(AGEA_compare_handler_args)
// {
//     return reflection::utils::default_compare<::agea::core::color>(from, to);
// }
//

//
// }  // namespace
//
// bool
// package::package_types_custom_loader::load(static_package& p)
// {
//     auto& reflection_regestry = ::agea::glob::reflection_type_registry::getr();
//     MAKE_POD_TYPE(agea::root::root__string, std::string);
//     MAKE_POD_TYPE(agea::root::root__bool, bool);
//
//     MAKE_POD_TYPE(agea::root::root__int8_t, std::int8_t);
//     MAKE_POD_TYPE(agea::root::root__int16_t, std::int16_t);
//     MAKE_POD_TYPE(agea::root::root__int32_t, std::int32_t);
//     MAKE_POD_TYPE(agea::root::root__int64_t, std::int64_t);
//
//     MAKE_POD_TYPE(agea::root::root__uint8_t, std::uint8_t);
//     MAKE_POD_TYPE(agea::root::root__uint16_t, std::uint16_t);
//     MAKE_POD_TYPE(agea::root::root__uint32_t, std::uint32_t);
//     MAKE_POD_TYPE(agea::root::root__uint64_t, std::uint64_t);
//
//     MAKE_POD_TYPE(agea::root::root__float, float);
//     MAKE_POD_TYPE(agea::root::root__double, double);
//
//     {
//         auto rt = reflection_regestry.get_type(root__id);
//
//         rt->deserialization = deserialize_t_id;
//         rt->compare = default_compare<utils::id>;
//         rt->copy = default_copy<utils::id>;
//         rt->serialization = serialize_t_id;
//         rt->ui = to_string_t_id;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__buffer);
//
//         rt->deserialization = deserialize_t_buf;
//         rt->copy = default_copy<utils::buffer>;
//         rt->serialization = serialize_t_buf;
//         rt->ui = default_to_string;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__vec3);
//
//         rt->deserialization = deserialize_t_vec3;
//         rt->compare = default_compare<glm::vec3>;
//         rt->copy = default_copy<glm::vec3>;
//         rt->serialization = serialize_t_vec3;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__smart_object);
//
//         rt->deserialization = deserialize_t_obj;
//         rt->compare = default_compare<root::smart_object*>;
//         rt->copy = copy_smart_object;
//         rt->serialization = serialize_t_obj;
//         rt->deserialization_with_proto = deserialize_from_proto_t_obj;
//         rt->ui = to_string_t_obj;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__component);
//
//         rt->deserialization = deserialize_t_com;
//         rt->compare = default_compare<root::smart_object*>;
//         rt->deserialization_with_proto = deserialize_from_proto_t_com;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__texture);
//
//         rt->deserialization = deserialize_t_txt;
//         rt->serialization = serialize_t_txt;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__material);
//
//         rt->deserialization = deserialize_t_mat;
//         rt->serialization = serialize_t_mat;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__mesh);
//
//         rt->deserialization = deserialize_t_msh;
//         rt->serialization = serialize_t_msh;
//     }
//     {
//         auto rt = reflection_regestry.get_type(root__shader_effect);
//
//         rt->deserialization = deserialize_t_se;
//         rt->serialization = serialize_t_se;
//     }
//
//     {
//         auto rt = reflection_regestry.get_type(root__vec2);
//         rt->ui = to_string_t_vec2;
//     }
//
//     {
//         auto rt = reflection_regestry.get_type(root__vec3);
//         rt->ui = to_string_t_vec3;
//     }
//
//     {
//         auto rt = reflection_regestry.get_type(root__vec4);
//         rt->ui = to_string_t_vec4;
//     }
//
//     return true;
// }
//
// }  // namespace agea::root

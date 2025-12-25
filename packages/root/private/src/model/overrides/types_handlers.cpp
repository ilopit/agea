#include "packages/root/model/overrides/properties_handlers.h"

#include "packages/root/package.root.h"

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/shader_effect.h"

#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/global_state.h>

#include <utils/agea_log.h>
#include <utils/string_utility.h>

#include <serialization/serialization.h>
#include <utils/dynamic_object_builder.h>
#include <utils/static_initializer.h>

namespace agea::root
{

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  core::object_load_context& occ,
                  core::architype a_type)
{
    auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    const auto id = AID(jc.as<std::string>());

    auto result = core::object_constructor::object_load_internal(id, occ);
    if (!result)
    {
        return result.error();
    }
    field = result.value();
    return result_code::ok;
}

result_code
color__save(reflection::type_save_context& ctx)
{
    auto& field = reflection::utils::as_type<::agea::root::color>(ctx.ptr);
    AGEA_unused(field);

    return result_code::ok;
}

result_code
color__load(reflection::type_load_context& ctx)
{
    auto str_color = ctx.jc->as<std::string>();

    if (str_color.size() != 0)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    uint8_t rgba[4] = {0, 0, 0, 255};

    agea::string_utils::convert_hext_string_to_bytes(8, str_color.data(), rgba);

    auto& field = reflection::utils::as_type<::agea::root::color>(ctx.ptr);

    field.m_data.r = rgba[0] ? (rgba[0] / 255.f) : 0.f;
    field.m_data.g = rgba[1] ? (rgba[1] / 255.f) : 0.f;
    field.m_data.b = rgba[2] ? (rgba[2] / 255.f) : 0.f;
    field.m_data.a = rgba[3] ? (rgba[3] / 255.f) : 0.f;

    return result_code::ok;
}

result_code
smart_obj__copy(reflection::type_copy_context& ctx)
{
    auto type = ctx.occ->get_construction_type();
    if (type != core::object_load_type::class_obj)
    {
        auto& obj = reflection::utils::as_type<::agea::root::smart_object*>(ctx.from);
        auto& dst = reflection::utils::as_type<::agea::root::smart_object*>(ctx.to);

        if (!dst)
        {
            return result_code::ok;
        }

        std::vector<root::smart_object*> objs;
        auto load_result = core::object_constructor::object_load(
            obj->get_id(), core::object_load_type::class_obj, *ctx.occ, objs);
        if (!load_result)
        {
            return load_result.error();
        }

        auto clone_result = core::object_constructor::object_clone_create_internal(
            obj->get_id(), obj->get_id(), *ctx.occ);
        if (!clone_result)
        {
            return clone_result.error();
        }
        dst = clone_result.value();
        return result_code::ok;
    }
    else
    {
        reflection::utils::cpp_default__copy<root::smart_object*>(ctx.from, ctx.to);
    }

    return result_code::ok;
}

result_code
smart_obj__instantiate(reflection::type_copy_context& ctx)
{
    auto& src_subobj = reflection::utils::as_type<::agea::root::smart_object*>(ctx.from);
    auto& dst_subobj = reflection::utils::as_type<::agea::root::smart_object*>(ctx.to);

    if (!src_subobj)
    {
        return result_code::ok;
    }

    AGEA_check(!dst_subobj, "dst_subobj");

    std::vector<root::smart_object*> objs;

    auto result = core::object_constructor::object_instantiate(*src_subobj, src_subobj->get_id(),
                                                               *ctx.occ, objs);
    if (!result)
    {
        return result.error();
    }
    dst_subobj = result.value();
    return result_code::ok;
}

agea::result_code
smart_obj__save(reflection::type_save_context& ctx)
{
    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ctx.ptr);

    (*ctx.jc)["id"] = field->get_id().str();

    return result_code::ok;
}

agea::result_code
smart_obj__load(reflection::type_load_context& ctx)
{
    return load_smart_object(ctx.ptr, *ctx.jc, *ctx.occ, core::architype::smart_object);
}

agea::result_code
smart_obj__to_string(reflection::type_ui_context& ctx)
{
    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ctx.ptr);

    *ctx.result = field ? field->get_id().str() : "empty";

    return result_code::ok;
}

agea::result_code
smart_obj__compare(reflection::type_compare_context& ctx)
{
    AGEA_unused(ctx);
    return result_code::failed;
}

result_code
texture_sample__save(reflection::type_save_context& ctx)
{
    AGEA_unused(ctx);
    return result_code::ok;
}

result_code
texture_sample__compare(reflection::type_compare_context& ctx)
{
    AGEA_unused(ctx);
    return result_code::ok;
}

result_code
texture_sample__copy(reflection::type_copy_context& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    dst->set_sample(src->get_id(), src->get_sample(src->get_id()));

    auto clone_result = core::object_constructor::object_clone_create_internal(
        src->get_sample(src->get_id()).txt->get_id(), src->get_sample(src->get_id()).txt->get_id(),
        *ctx.occ);

    if (!clone_result)
    {
        return clone_result.error();
    }

    dst->get_sample(src->get_id()).txt = clone_result.value()->as<root::texture>();

    return result_code::ok;
}

result_code
texture_sample__instantiate(reflection::type_copy_context& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    dst->set_sample(src->get_id(), src->get_sample(src->get_id()));

    auto clone_result = core::object_constructor::object_clone_create_internal(
        src->get_sample(src->get_id()).txt->get_id(), src->get_sample(src->get_id()).txt->get_id(),
        *ctx.occ);

    if (!clone_result)
    {
        return clone_result.error();
    }

    dst->get_sample(src->get_id()).txt = clone_result.value()->as<root::texture>();

    return result_code::ok;
}

result_code
texture_sample__load(reflection::type_load_context& ctx)
{
    auto src = ctx.obj->as<root::material>();

    const auto texture_id = AID((*ctx.jc)["texture"].as<std::string>());

    auto result = core::object_constructor::object_load_internal(texture_id, *ctx.occ);

    if (!result)
    {
        ALOG_ERROR("Texture doesn't exist");
        return result.error();
    }

    auto loaded_obj = result.value();

    const auto slot = (*ctx.jc)["slot"].as<uint32_t>();

    auto& sample = src->get_sample(src->get_id());
    sample.txt = loaded_obj->as<root::texture>();
    sample.sampler_id = AID((*ctx.jc)["sampler"].as<std::string>());
    sample.slot = slot;

    return result_code::ok;
}

agea::result_code
buffer__save(reflection::type_save_context& ctx)
{
    auto& field = reflection::utils::as_type<::agea::utils::buffer>(ctx.ptr);

    auto package_path = ctx.obj->get_package()->get_relative_path(field.get_file());

    if (!utils::buffer::save(field))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    *ctx.jc = package_path.str();

    return result_code::ok;
}

agea::result_code
buffer__load(reflection::type_load_context& ctx)
{
    auto rel_path = APATH(ctx.jc->as<std::string>());

    utils::path package_path;

    if (!ctx.occ->make_full_path(rel_path, package_path))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto& f = reflection::utils::as_type<::agea::utils::buffer>(ctx.ptr);
    f.set_file(package_path);

    if (!utils::buffer::load(package_path, f))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return result_code::ok;
}

agea::result_code
buffer__copy(reflection::type_copy_context& ctx)
{
    reflection::utils::cpp_default__copy<::agea::utils::buffer>(ctx.from, ctx.to);

    return result_code::ok;
}

agea::result_code
buffer__to_string(reflection::type_ui_context& ctx)
{
    auto& field = reflection::utils::as_type<::agea::utils::buffer>(ctx.ptr);

    *ctx.result = field.get_file().str();

    return result_code::ok;
}

//
// // ========  ID  ====================================
agea::result_code
id__save(reflection::type_save_context& ctx)
{
    *ctx.jc = agea::reflection::utils::as_type<agea::utils::id>(ctx.ptr).str();
    return agea::result_code::ok;
}

agea::result_code
id__load(reflection::type_load_context& ctx)
{
    agea::reflection::utils::as_type<agea::utils::id>(ctx.ptr) = AID(ctx.jc->as<std::string>());
    return agea::result_code::ok;
}

agea::result_code
id__to_string(reflection::type_ui_context& ctx)
{
    *ctx.result = agea::reflection::utils::as_type<agea::utils::id>(ctx.ptr).str();
    return agea::result_code::ok;
}

// ========  VEC2  ====================================

agea::result_code
vec2__to_string(reflection::type_ui_context& ctx)
{
    auto& field = agea::reflection::utils::as_type<::agea::root::vec3>(ctx.ptr);

    *ctx.result = std::format("{} {}", field.x, field.y);

    return agea::result_code::ok;
}

// ========  VEC3  ====================================
agea::result_code
vec3__save(reflection::type_save_context& ctx)
{
    auto& field = agea::reflection::utils::as_type<agea::root::vec3>(ctx.ptr);

    (*ctx.jc)["x"] = field.x;
    (*ctx.jc)["y"] = field.y;
    (*ctx.jc)["z"] = field.z;

    return agea::result_code::ok;
}

agea::result_code
vec3__load(reflection::type_load_context& ctx)
{
    auto& field = agea::reflection::utils::as_type<agea::root::vec3>(ctx.ptr);

    field.x = (*ctx.jc)["x"].as<float>();
    field.y = (*ctx.jc)["y"].as<float>();
    field.z = (*ctx.jc)["z"].as<float>();

    return agea::result_code::ok;
}

agea::result_code
vec3__to_string(reflection::type_ui_context& ctx)
{
    auto& field = agea::reflection::utils::as_type<::agea::root::vec3>(ctx.ptr);

    *ctx.result = std::format("{} {} {}", field.x, field.y, field.z);

    return agea::result_code::ok;
}

// ========  VEC4  ====================================

agea::result_code
vec4__to_string(reflection::type_ui_context& ctx)
{
    auto& field = agea::reflection::utils::as_type<::agea::root::vec4>(ctx.ptr);

    *ctx.result = std::format("{} {} {} {}", field.x, field.y, field.z, field.w);

    return agea::result_code::ok;
}

}  // namespace agea::root
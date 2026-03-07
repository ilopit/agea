#include "packages/root/model/overrides/properties_handlers.h"

#include "packages/root/package.root.h"

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/sampler.h"
#include "packages/root/model/assets/shader_effect.h"

#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>

#include <utils/kryga_log.h>
#include <utils/string_utility.h>

#include <serialization/serialization.h>
#include <utils/dynamic_object_builder.h>

namespace kryga::root
{

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::container& jc,
                  core::object_constructor& ctor,
                  core::architype a_type)
{
    auto& field = reflection::utils::as_type<::kryga::root::smart_object*>(ptr);

    const auto id = AID(jc.as<std::string>());

    auto result = ctor.load_obj(id);
    if (!result)
    {
        return result.error();
    }
    field = result.value();
    return result_code::ok;
}

result_code
color__save(reflection::type_context__save& ctx)
{
    auto& field = reflection::utils::as_type<::kryga::root::color>(ctx.obj);
    KRG_unused(field);

    return result_code::ok;
}

result_code
color__load(reflection::type_context__load& ctx)
{
    auto str_color = ctx.jc->as<std::string>();

    if (str_color.empty())
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    uint8_t rgba[4] = {0, 0, 0, 255};

    kryga::string_utils::convert_hex_string_to_bytes(8, str_color.data(), rgba);

    auto& field = reflection::utils::as_type<::kryga::root::color>(ctx.obj);

    field.m_data.r = rgba[0] ? (rgba[0] / 255.f) : 0.f;
    field.m_data.g = rgba[1] ? (rgba[1] / 255.f) : 0.f;
    field.m_data.b = rgba[2] ? (rgba[2] / 255.f) : 0.f;
    field.m_data.a = rgba[3] ? (rgba[3] / 255.f) : 0.f;

    return result_code::ok;
}

result_code
smart_obj__copy(reflection::type_context__copy& ctx)
{
    reflection::utils::cpp_default__copy<root::smart_object*>(ctx.src_obj, ctx.dst_obj);

    return result_code::ok;
}

result_code
smart_obj__instantiate(reflection::type_context__copy& ctx)
{
    auto& src_subobj = reflection::utils::as_type<::kryga::root::smart_object*>(ctx.src_obj);
    auto& dst_subobj = reflection::utils::as_type<::kryga::root::smart_object*>(ctx.dst_obj);

    if (!src_subobj)
    {
        return result_code::ok;
    }

    KRG_check(!dst_subobj, "dst_subobj");

    auto result = ctx.ctor->instantiate_obj(*src_subobj, src_subobj->get_id());
    if (!result)
    {
        return result.error();
    }
    dst_subobj = result.value();
    return result_code::ok;
}

kryga::result_code
smart_obj__save(reflection::type_context__save& ctx)
{
    auto field = reflection::utils::as_type<::kryga::root::smart_object*>(ctx.obj);

    (*ctx.jc)["id"] = field->get_id().str();

    return result_code::ok;
}

kryga::result_code
smart_obj__load(reflection::type_context__load& ctx)
{
    return load_smart_object(ctx.obj, *ctx.jc, *ctx.ctor, core::architype::smart_object);
}

kryga::result_code
smart_obj__to_string(reflection::type_context__to_string& ctx)
{
    auto field = reflection::utils::as_type<::kryga::root::smart_object*>(ctx.obj);

    *ctx.result = field ? field->get_id().str() : "empty";

    return result_code::ok;
}

kryga::result_code
smart_obj__compare(reflection::type_context__compare& ctx)
{
    KRG_unused(ctx);
    return result_code::failed;
}

result_code
texture_slot__save(reflection::type_context__save& ctx)
{
    KRG_unused(ctx);
    return result_code::ok;
}

result_code
texture_slot__compare(reflection::type_context__compare& ctx)
{
    KRG_unused(ctx);
    return result_code::ok;
}

result_code
texture_slot__copy(reflection::type_context__copy& ctx)
{
    auto& src = reflection::utils::as_type<::kryga::root::material*>(ctx.src_obj);
    auto& dst = reflection::utils::as_type<::kryga::root::material*>(ctx.dst_obj);

    auto& src_slot = src->get_slot(src->get_id());
    dst->set_slot(src->get_id(), src_slot);

    // Clone texture
    if (src_slot.txt)
    {
        auto clone_result = ctx.ctor->clone_obj(*src_slot.txt, src_slot.txt->get_id());

        if (!clone_result)
        {
            return clone_result.error();
        }

        dst->get_slot(src->get_id()).txt = clone_result.value()->as<root::texture>();
    }

    // Samplers are shared assets - just copy pointer
    dst->get_slot(src->get_id()).smp = src_slot.smp;

    return result_code::ok;
}

result_code
texture_slot__instantiate(reflection::type_context__copy& ctx)
{
    auto& src = reflection::utils::as_type<::kryga::root::material*>(ctx.src_obj);
    auto& dst = reflection::utils::as_type<::kryga::root::material*>(ctx.dst_obj);

    auto& src_slot = src->get_slot(src->get_id());
    dst->set_slot(src->get_id(), src_slot);

    // Clone texture
    if (src_slot.txt)
    {
        auto clone_result = ctx.ctor->clone_obj(*src_slot.txt, src_slot.txt->get_id());

        if (!clone_result)
        {
            return clone_result.error();
        }

        dst->get_slot(src->get_id()).txt = clone_result.value()->as<root::texture>();
    }

    // Samplers are shared assets - just copy pointer
    dst->get_slot(src->get_id()).smp = src_slot.smp;

    return result_code::ok;
}

result_code
texture_slot__load(reflection::type_context__load& ctx)
{
    auto& src = reflection::utils::as_type<::kryga::root::material*>(ctx.obj);

    const auto texture_id = AID((*ctx.jc)["texture"].as<std::string>());

    // Load texture
    auto tex_result = ctx.ctor->load_obj(texture_id);
    if (!tex_result)
    {
        ALOG_ERROR("Texture doesn't exist: {}", texture_id.str());
        return tex_result.error();
    }

    auto loaded_obj = tex_result.value();

    const auto slot = (*ctx.jc)["slot"].as<uint32_t>();

    auto& tex_slot = src->get_slot(src->get_id());
    tex_slot.txt = loaded_obj->as<root::texture>();
    tex_slot.slot = slot;

    // Load sampler (optional)
    auto& jc = *ctx.jc;
    if (jc["sampler"] && jc["sampler"].IsScalar())
    {
        const auto sampler_id = AID(jc["sampler"].as<std::string>());
        auto smp_result = ctx.ctor->load_obj(sampler_id);
        if (smp_result)
        {
            tex_slot.smp = smp_result.value()->as<root::sampler>();
        }
        else
        {
            ALOG_WARN("Sampler not found: {}, using default", sampler_id.str());
            tex_slot.smp = nullptr;
        }
    }
    else
    {
        tex_slot.smp = nullptr;
    }

    return result_code::ok;
}

kryga::result_code
buffer__save(reflection::type_context__save& ctx)
{
    auto& field = reflection::utils::as_type<::kryga::utils::buffer>(ctx.obj);

    auto package_path = ctx.owner_obj->get_package()->get_relative_path(field.get_file());

    if (!utils::buffer::save(field))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    *ctx.jc = package_path.str();

    return result_code::ok;
}

kryga::result_code
buffer__load(reflection::type_context__load& ctx)
{
    auto rel_path = APATH(ctx.jc->as<std::string>());

    utils::path package_path;

    if (!ctx.ctor->get_olc()->make_full_path(rel_path, package_path))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto& f = reflection::utils::as_type<::kryga::utils::buffer>(ctx.obj);
    f.set_file(package_path);

    if (!utils::buffer::load(package_path, f))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return result_code::ok;
}

kryga::result_code
buffer__copy(reflection::type_context__copy& ctx)
{
    reflection::utils::cpp_default__copy<::kryga::utils::buffer>(ctx.src_obj, ctx.dst_obj);

    return result_code::ok;
}

kryga::result_code
buffer__to_string(reflection::type_context__to_string& ctx)
{
    auto& field = reflection::utils::as_type<::kryga::utils::buffer>(ctx.obj);

    *ctx.result = field.get_file().str();

    return result_code::ok;
}

//
// // ========  ID  ====================================
kryga::result_code
id__save(reflection::type_context__save& ctx)
{
    *ctx.jc = kryga::reflection::utils::as_type<kryga::utils::id>(ctx.obj).str();
    return kryga::result_code::ok;
}

kryga::result_code
id__load(reflection::type_context__load& ctx)
{
    kryga::reflection::utils::as_type<kryga::utils::id>(ctx.obj) = AID(ctx.jc->as<std::string>());
    return kryga::result_code::ok;
}

kryga::result_code
id__to_string(reflection::type_context__to_string& ctx)
{
    *ctx.result = kryga::reflection::utils::as_type<kryga::utils::id>(ctx.obj).str();
    return kryga::result_code::ok;
}

// ========  VEC2  ====================================

kryga::result_code
vec2__to_string(reflection::type_context__to_string& ctx)
{
    auto& field = kryga::reflection::utils::as_type<::kryga::root::vec2>(ctx.obj);

    *ctx.result = std::format("{} {}", field.x, field.y);

    return kryga::result_code::ok;
}

// ========  VEC3  ====================================
kryga::result_code
vec3__save(reflection::type_context__save& ctx)
{
    auto& field = kryga::reflection::utils::as_type<kryga::root::vec3>(ctx.obj);

    (*ctx.jc)["x"] = field.x;
    (*ctx.jc)["y"] = field.y;
    (*ctx.jc)["z"] = field.z;

    return kryga::result_code::ok;
}

kryga::result_code
vec3__load(reflection::type_context__load& ctx)
{
    auto& field = kryga::reflection::utils::as_type<kryga::root::vec3>(ctx.obj);

    field.x = (*ctx.jc)["x"].as<float>();
    field.y = (*ctx.jc)["y"].as<float>();
    field.z = (*ctx.jc)["z"].as<float>();

    return kryga::result_code::ok;
}

kryga::result_code
vec3__to_string(reflection::type_context__to_string& ctx)
{
    auto& field = kryga::reflection::utils::as_type<::kryga::root::vec3>(ctx.obj);

    *ctx.result = std::format("{} {} {}", field.x, field.y, field.z);

    return kryga::result_code::ok;
}

// ========  VEC4  ====================================

kryga::result_code
vec4__to_string(reflection::type_context__to_string& ctx)
{
    auto& field = kryga::reflection::utils::as_type<::kryga::root::vec4>(ctx.obj);

    *ctx.result = std::format("{} {} {} {}", field.x, field.y, field.z, field.w);

    return kryga::result_code::ok;
}

}  // namespace kryga::root
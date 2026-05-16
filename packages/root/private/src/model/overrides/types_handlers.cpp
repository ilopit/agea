#include "packages/root/model/overrides/properties_handlers.h"

#include "packages/root/package.root.h"

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/sampler.h"
#include "packages/root/model/assets/shader_effect.h"

#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>

#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/rid.h>
#include <vfs/io.h>

#include <cstring>

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

    const auto id_str = jc.as<std::string>();
    if (id_str.empty())
    {
        field = nullptr;
        return result_code::ok;
    }

    auto result = ctor.load_obj(AID(id_str));
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

    // Must match load: smart_obj__load reads jc.as<std::string>() expecting a
    // scalar id. Writing a mapping breaks round-trip for converter output.
    // Null pointer round-trips as empty string (see load_smart_object).
    *ctx.jc = field ? field->get_id().str() : std::string{};

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
    auto left = reflection::utils::as_type<::kryga::root::smart_object*>(ctx.left_obj);
    auto right = reflection::utils::as_type<::kryga::root::smart_object*>(ctx.right_obj);

    if (left == right)
    {
        return result_code::ok;
    }
    if (!left || !right)
    {
        return result_code::failed;
    }
    return left->get_id() == right->get_id() ? result_code::ok : result_code::failed;
}

result_code
texture_slot__save(reflection::type_context__save& ctx)
{
    auto& ts = reflection::utils::as_type<texture_slot>(ctx.obj);
    if (!ts.txt)
    {
        return result_code::ok;
    }

    auto& sc = *ctx.jc;
    sc["texture"] = ts.txt->get_id().str();
    sc["slot"] = ts.slot;
    if (ts.smp)
    {
        sc["sampler"] = ts.smp->get_id().str();
    }

    return result_code::ok;
}

result_code
texture_slot__compare(reflection::type_context__compare& ctx)
{
    auto& left = reflection::utils::as_type<texture_slot>(ctx.left_obj);
    auto& right = reflection::utils::as_type<texture_slot>(ctx.right_obj);

    bool left_empty = !left.txt;
    bool right_empty = !right.txt;

    if (left_empty && right_empty)
    {
        return result_code::ok;
    }
    if (left_empty != right_empty)
    {
        return result_code::failed;
    }
    if (left.txt->get_id() != right.txt->get_id())
    {
        return result_code::failed;
    }
    if (left.slot != right.slot)
    {
        return result_code::failed;
    }

    bool left_smp = left.smp != nullptr;
    bool right_smp = right.smp != nullptr;
    if (left_smp != right_smp)
    {
        return result_code::failed;
    }
    if (left_smp && right_smp && left.smp->get_id() != right.smp->get_id())
    {
        return result_code::failed;
    }

    return result_code::ok;
}

result_code
texture_slot__copy(reflection::type_context__copy& ctx)
{
    auto& src = reflection::utils::as_type<texture_slot>(ctx.src_obj);
    auto& dst = reflection::utils::as_type<texture_slot>(ctx.dst_obj);

    dst = src;

    if (src.txt)
    {
        auto clone_result = ctx.ctor->clone_obj(*src.txt, src.txt->get_id());
        if (!clone_result)
        {
            return clone_result.error();
        }
        dst.txt = clone_result.value()->as<root::texture>();
    }

    return result_code::ok;
}

result_code
texture_slot__instantiate(reflection::type_context__copy& ctx)
{
    auto& src = reflection::utils::as_type<texture_slot>(ctx.src_obj);
    auto& dst = reflection::utils::as_type<texture_slot>(ctx.dst_obj);

    dst = src;

    if (src.txt)
    {
        root::smart_object* obj = ctx.ctor->get_olc()->find_obj(src.txt->get_id());
        if (!obj)
        {
            auto result = ctx.ctor->instantiate_obj(*src.txt, src.txt->get_id());
            if (!result)
            {
                return result.error();
            }
            obj = result.value();
        }
        dst.txt = obj->as<root::texture>();
    }

    return result_code::ok;
}

result_code
texture_slot__load(reflection::type_context__load& ctx)
{
    auto& ts = reflection::utils::as_type<texture_slot>(ctx.obj);
    auto& jc = *ctx.jc;

    if (!jc["texture"] || !jc["texture"].IsScalar())
    {
        ts.txt = nullptr;
        ts.slot = 0;
        ts.smp = nullptr;
        return result_code::ok;
    }

    const auto texture_id = AID(jc["texture"].as<std::string>());

    auto tex_result = ctx.ctor->load_obj(texture_id);
    if (!tex_result)
    {
        ALOG_ERROR("Texture doesn't exist: {}", texture_id.str());
        return tex_result.error();
    }

    ts.txt = tex_result.value()->as<root::texture>();
    ts.slot = jc["slot"].as<uint32_t>();

    if (jc["sampler"] && jc["sampler"].IsScalar())
    {
        const auto sampler_id = AID(jc["sampler"].as<std::string>());
        auto smp_result = ctx.ctor->load_obj(sampler_id);
        if (smp_result)
        {
            ts.smp = smp_result.value()->as<root::sampler>();
        }
        else
        {
            ALOG_WARN("Sampler not found: {}, using default", sampler_id.str());
            ts.smp = nullptr;
        }
    }
    else
    {
        ts.smp = nullptr;
    }

    return result_code::ok;
}

kryga::result_code
buffer__compare(reflection::type_context__compare& ctx)
{
    auto& a = reflection::utils::as_type<::kryga::utils::buffer>(ctx.left_obj);
    auto& b = reflection::utils::as_type<::kryga::utils::buffer>(ctx.right_obj);
    if (a.size() != b.size())
    {
        return result_code::failed;
    }
    if (a.size() == 0)
    {
        return result_code::ok;
    }
    return std::memcmp(a.data(), b.data(), a.size()) == 0 ? result_code::ok : result_code::failed;
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

    vfs::rid rid;
    if (!ctx.ctor->get_olc()->resolve(rel_path, rid))
    {
        ALOG_ERROR("buffer__load: resolve failed for '{}'", rel_path.str());
        return result_code::failed;
    }

    // Read via VFS so APK-asset backends on Android work uniformly with
    // physical backends on desktop — no `real_path` requirement.
    auto& f = reflection::utils::as_type<::kryga::utils::buffer>(ctx.obj);
    if (!vfs::load_buffer(rid, f))
    {
        ALOG_ERROR("buffer__load: VFS load failed for '{}'", rid.str());
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
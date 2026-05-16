#include "packages/root/editor/overrides/editor_types_handlers.h"

#include <packages/root/model/core_types/vec2.h>
#include <packages/root/model/core_types/vec3.h>
#include <packages/root/model/core_types/vec4.h>
#include <packages/root/model/assets/texture_slot.h>
#include <packages/root/model/assets/texture.h>
#include <packages/root/model/assets/sampler.h>

#include <core/model_system.h>
#include <core/caches/caches_map.h>
#include <core/model_system.h>
#include <core/reflection/reflection_type_utils.h>

#include <global_state/global_state.h>

#include <json/json.h>

#include <utils/id.h>

#include <string>

namespace kryga::root
{

// ========  PRIMITIVES  ==============================

kryga::result_code
bool__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(*reinterpret_cast<const bool*>(ctx.obj));
    return kryga::result_code::ok;
}

kryga::result_code
bool__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isBool() && !ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<bool*>(ctx.obj) = ctx.jc->asBool();
    return kryga::result_code::ok;
}

kryga::result_code
int8__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(static_cast<int>(*reinterpret_cast<const int8_t*>(ctx.obj)));
    return kryga::result_code::ok;
}

kryga::result_code
int8__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<int8_t*>(ctx.obj) = static_cast<int8_t>(ctx.jc->asInt());
    return kryga::result_code::ok;
}

kryga::result_code
int16__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(static_cast<int>(*reinterpret_cast<const int16_t*>(ctx.obj)));
    return kryga::result_code::ok;
}

kryga::result_code
int16__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<int16_t*>(ctx.obj) = static_cast<int16_t>(ctx.jc->asInt());
    return kryga::result_code::ok;
}

kryga::result_code
int32__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(*reinterpret_cast<const int32_t*>(ctx.obj));
    return kryga::result_code::ok;
}

kryga::result_code
int32__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<int32_t*>(ctx.obj) = ctx.jc->asInt();
    return kryga::result_code::ok;
}

kryga::result_code
int64__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(static_cast<Json::Int64>(*reinterpret_cast<const int64_t*>(ctx.obj)));
    return kryga::result_code::ok;
}

kryga::result_code
int64__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<int64_t*>(ctx.obj) = ctx.jc->asInt64();
    return kryga::result_code::ok;
}

kryga::result_code
uint8__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(static_cast<unsigned>(*reinterpret_cast<const uint8_t*>(ctx.obj)));
    return kryga::result_code::ok;
}

kryga::result_code
uint8__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<uint8_t*>(ctx.obj) = static_cast<uint8_t>(ctx.jc->asUInt());
    return kryga::result_code::ok;
}

kryga::result_code
uint16__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(static_cast<unsigned>(*reinterpret_cast<const uint16_t*>(ctx.obj)));
    return kryga::result_code::ok;
}

kryga::result_code
uint16__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<uint16_t*>(ctx.obj) = static_cast<uint16_t>(ctx.jc->asUInt());
    return kryga::result_code::ok;
}

kryga::result_code
uint32__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(*reinterpret_cast<const uint32_t*>(ctx.obj));
    return kryga::result_code::ok;
}

kryga::result_code
uint32__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<uint32_t*>(ctx.obj) = ctx.jc->asUInt();
    return kryga::result_code::ok;
}

kryga::result_code
uint64__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(static_cast<Json::UInt64>(*reinterpret_cast<const uint64_t*>(ctx.obj)));
    return kryga::result_code::ok;
}

kryga::result_code
uint64__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<uint64_t*>(ctx.obj) = ctx.jc->asUInt64();
    return kryga::result_code::ok;
}

kryga::result_code
float__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(*reinterpret_cast<const float*>(ctx.obj));
    return kryga::result_code::ok;
}

kryga::result_code
float__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<float*>(ctx.obj) = ctx.jc->asFloat();
    return kryga::result_code::ok;
}

kryga::result_code
double__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(*reinterpret_cast<const double*>(ctx.obj));
    return kryga::result_code::ok;
}

kryga::result_code
double__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isNumeric())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<double*>(ctx.obj) = ctx.jc->asDouble();
    return kryga::result_code::ok;
}

kryga::result_code
string__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(*reinterpret_cast<const std::string*>(ctx.obj));
    return kryga::result_code::ok;
}

kryga::result_code
string__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isString())
    {
        return kryga::result_code::failed;
    }
    *reinterpret_cast<std::string*>(ctx.obj) = ctx.jc->asString();
    return kryga::result_code::ok;
}

// ========  ID  ======================================

kryga::result_code
id__json_save(reflection::type_context__json_save& ctx)
{
    *ctx.jc = Json::Value(kryga::reflection::utils::as_type<kryga::utils::id>(ctx.obj).str());
    return kryga::result_code::ok;
}

kryga::result_code
id__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isString())
    {
        return kryga::result_code::failed;
    }
    auto s = ctx.jc->asString();
    kryga::reflection::utils::as_type<kryga::utils::id>(ctx.obj) =
        s.empty() ? kryga::utils::id() : AID(s);
    return kryga::result_code::ok;
}

// ========  VEC2  ====================================

kryga::result_code
vec2__json_save(reflection::type_context__json_save& ctx)
{
    const auto& v = kryga::reflection::utils::as_type<::kryga::root::vec2>(ctx.obj);
    *ctx.jc = Json::Value(Json::arrayValue);
    ctx.jc->append(v.x);
    ctx.jc->append(v.y);
    return kryga::result_code::ok;
}

kryga::result_code
vec2__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isArray() || ctx.jc->size() != 2)
    {
        return kryga::result_code::failed;
    }
    for (Json::ArrayIndex i = 0; i < 2; ++i)
    {
        if (!(*ctx.jc)[i].isNumeric())
        {
            return kryga::result_code::failed;
        }
    }
    auto& dst = kryga::reflection::utils::as_type<::kryga::root::vec2>(ctx.obj);
    dst.x = (*ctx.jc)[0].asFloat();
    dst.y = (*ctx.jc)[1].asFloat();
    return kryga::result_code::ok;
}

// ========  VEC3  ====================================

kryga::result_code
vec3__json_save(reflection::type_context__json_save& ctx)
{
    const auto& v = kryga::reflection::utils::as_type<::kryga::root::vec3>(ctx.obj);
    *ctx.jc = Json::Value(Json::arrayValue);
    ctx.jc->append(v.x);
    ctx.jc->append(v.y);
    ctx.jc->append(v.z);
    return kryga::result_code::ok;
}

kryga::result_code
vec3__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isArray() || ctx.jc->size() != 3)
    {
        return kryga::result_code::failed;
    }
    for (Json::ArrayIndex i = 0; i < 3; ++i)
    {
        if (!(*ctx.jc)[i].isNumeric())
        {
            return kryga::result_code::failed;
        }
    }
    auto& dst = kryga::reflection::utils::as_type<::kryga::root::vec3>(ctx.obj);
    dst.x = (*ctx.jc)[0].asFloat();
    dst.y = (*ctx.jc)[1].asFloat();
    dst.z = (*ctx.jc)[2].asFloat();
    return kryga::result_code::ok;
}

// ========  VEC4  ====================================

kryga::result_code
vec4__json_save(reflection::type_context__json_save& ctx)
{
    const auto& v = kryga::reflection::utils::as_type<::kryga::root::vec4>(ctx.obj);
    *ctx.jc = Json::Value(Json::arrayValue);
    ctx.jc->append(v.x);
    ctx.jc->append(v.y);
    ctx.jc->append(v.z);
    ctx.jc->append(v.w);
    return kryga::result_code::ok;
}

kryga::result_code
vec4__json_load(reflection::type_context__json_load& ctx)
{
    if (!ctx.jc->isArray() || ctx.jc->size() != 4)
    {
        return kryga::result_code::failed;
    }
    for (Json::ArrayIndex i = 0; i < 4; ++i)
    {
        if (!(*ctx.jc)[i].isNumeric())
        {
            return kryga::result_code::failed;
        }
    }
    auto& dst = kryga::reflection::utils::as_type<::kryga::root::vec4>(ctx.obj);
    dst.x = (*ctx.jc)[0].asFloat();
    dst.y = (*ctx.jc)[1].asFloat();
    dst.z = (*ctx.jc)[2].asFloat();
    dst.w = (*ctx.jc)[3].asFloat();
    return kryga::result_code::ok;
}

// ========  TEXTURE_SLOT  ===========================

kryga::result_code
texture_slot__json_save(reflection::type_context__json_save& ctx)
{
    auto& ts = kryga::reflection::utils::as_type<texture_slot>(ctx.obj);
    auto& jc = *ctx.jc;
    jc = Json::Value(Json::objectValue);
    jc["texture"] = ts.txt ? ts.txt->get_id().str() : std::string();
    jc["slot"] = ts.slot;
    jc["sampler"] = ts.smp ? ts.smp->get_id().str() : std::string();
    return kryga::result_code::ok;
}

static root::smart_object*
find_texture(const utils::id& id)
{
    auto& model = glob::glob_state().getr_model();
    auto* obj = model.class_caches.textures.get_item(id);
    if (!obj)
    {
        obj = model.instance_caches.textures.get_item(id);
    }
    return obj;
}

static root::smart_object*
find_sampler(const utils::id& id)
{
    auto& model = glob::glob_state().getr_model();
    auto* obj = model.class_caches.samplers.get_item(id);
    if (!obj)
    {
        obj = model.instance_caches.samplers.get_item(id);
    }
    return obj;
}

kryga::result_code
texture_slot__json_load(reflection::type_context__json_load& ctx)
{
    auto& ts = kryga::reflection::utils::as_type<texture_slot>(ctx.obj);

    if (ctx.jc->isString())
    {
        auto tex_id_str = ctx.jc->asString();
        if (tex_id_str.empty())
        {
            ts.txt = nullptr;
            return kryga::result_code::ok;
        }
        auto* tex_obj = find_texture(AID(tex_id_str));
        if (!tex_obj)
        {
            return kryga::result_code::failed;
        }
        ts.txt = tex_obj->as<texture>();
        return kryga::result_code::ok;
    }

    if (!ctx.jc->isObject())
    {
        return kryga::result_code::failed;
    }

    auto& jc = *ctx.jc;
    auto tex_str = jc.get("texture", "").asString();
    if (tex_str.empty())
    {
        ts.txt = nullptr;
    }
    else
    {
        auto* tex_obj = find_texture(AID(tex_str));
        if (!tex_obj)
        {
            return kryga::result_code::failed;
        }
        ts.txt = tex_obj->as<texture>();
    }

    if (jc.isMember("slot"))
    {
        ts.slot = jc["slot"].asUInt();
    }

    auto smp_str = jc.get("sampler", "").asString();
    if (smp_str.empty())
    {
        ts.smp = nullptr;
    }
    else
    {
        auto* smp_obj = find_sampler(AID(smp_str));
        if (smp_obj)
        {
            ts.smp = smp_obj->as<sampler>();
        }
    }

    return kryga::result_code::ok;
}

}  // namespace kryga::root

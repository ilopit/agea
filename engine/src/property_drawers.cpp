#include "engine/property_drawers.h"

#include <imgui.h>

#include <inttypes.h>

#include "core/caches/materials_cache.h"
#include "core/caches/meshes_cache.h"

#include "root/assets/material.h"
#include "root/assets/mesh.h"
#include "root/assets/texture.h"

#include "core/reflection/reflection_type_utils.h"

#include "root/root_types_ids.ar.h"

namespace agea
{
namespace ui
{

void
property_drawers::init()
{
    using namespace reflection;
    // clang-format off

    ro_drawers()[root::root__string] = property_drawers::draw_ro_str;

    ro_drawers()[root::root__bool] = property_drawers::draw_ro_bool;

    ro_drawers()[root::root__int8_t] = property_drawers::draw_ro_i8;
    ro_drawers()[root::root__int16_t] = property_drawers::draw_ro_i16;
    ro_drawers()[root::root__int32_t] = property_drawers::draw_ro_i32;
    ro_drawers()[root::root__int64_t] = property_drawers::draw_ro_i64;


    ro_drawers()[root::root__uint8_t] = property_drawers::draw_ro_i8;
    ro_drawers()[root::root__uint16_t] = property_drawers::draw_ro_i16;
    ro_drawers()[root::root__uint32_t] = property_drawers::draw_ro_i32;
    ro_drawers()[root::root__uint64_t] = property_drawers::draw_ro_i64;


    ro_drawers()[root::root__float] = property_drawers::draw_ro_f;
    ro_drawers()[root::root__double] = property_drawers::draw_ro_d;

    ro_drawers()[root::root__vec3] = property_drawers::draw_ro_vec3;

    ro_drawers()[root::root__material] = property_drawers::draw_ro_mat;
    ro_drawers()[root::root__mesh] = property_drawers::draw_ro_msh;

    // clang-format on
}

result_code
property_drawers::draw_ro(::agea::root::smart_object* obj, ::agea::reflection::property& p)
{
    auto ptr = (::agea::blob_ptr)obj;

    ptr = ::agea::reflection::utils::reduce_ptr(ptr + p.offset, p.type.is_ptr);
    ro_drawers()[p.type.type_id](ptr);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_str(::agea::blob_ptr ptr)
{
    auto& p = agea::reflection::utils::as_type<std::string>(ptr);

    ImGui::Text("%s", p.c_str());

    return result_code::ok;
}

result_code
property_drawers::draw_ro_bool(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<bool>(ptr);
    ImGui::Text("%s", (p ? "true" : "false"));

    return result_code::ok;
}

result_code
property_drawers::draw_ro_i8(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::int8_t>(ptr);
    ImGui::Text("%" PRIi8 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_i16(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::int16_t>(ptr);
    ImGui::Text("%" PRIi16 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_i32(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::int32_t>(ptr);
    ImGui::Text("%" PRIi32 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_i64(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::int64_t>(ptr);
    ImGui::Text("%" PRIi64 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_u8(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::uint8_t>(ptr);
    ImGui::Text("%" PRIu8 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_u16(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::uint16_t>(ptr);
    ImGui::Text("%" PRIu16 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_u32(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::uint32_t>(ptr);
    ImGui::Text("%" PRIu32 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_u64(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<std::uint64_t>(ptr);
    ImGui::Text("%" PRIu64 "", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_f(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<float>(ptr);
    ImGui::Text("%f", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_d(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::utils::as_type<double>(ptr);
    ImGui::Text("%lf", p);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_vec3(::agea::blob_ptr ptr)
{
    auto& p = agea::reflection::utils::as_type<glm::vec3>(ptr);
    ImGui::Text("%f %f %f", (p)[0], (p)[1], (p)[2]);

    return result_code::ok;
}

result_code
property_drawers::draw_ro_mat(::agea::blob_ptr ptr)
{
    auto& t = agea::reflection::utils::as_type<std::shared_ptr<root::material>>(ptr);
    ImGui::Text("%s", t->get_id().cstr());

    return result_code::ok;
}

result_code
property_drawers::draw_ro_msh(::agea::blob_ptr ptr)
{
    auto& t = agea::reflection::utils::as_type<root::mesh*>(ptr);
    ImGui::Text("%s", t->get_id().cstr());

    return result_code::ok;
}
}  // namespace ui
}  // namespace agea
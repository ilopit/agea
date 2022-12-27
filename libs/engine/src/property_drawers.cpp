#include "engine/property_drawers.h"

#include <imgui.h>

#include <inttypes.h>

#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"

#include "model/assets/material.h"
#include "model/assets/mesh.h"
#include "model/assets/texture.h"

namespace agea
{
namespace ui
{

void
property_drawers::init()
{
    using namespace reflection;
    // clang-format off
    ro_drawers().resize((size_t)utils::agea_type::t_last, nullptr);

    ro_drawers()[(size_t)utils::agea_type::t_str] = property_drawers::draw_ro_str;

    ro_drawers()[(size_t)utils::agea_type::t_bool] = property_drawers::draw_ro_bool;

    ro_drawers()[(size_t)utils::agea_type::t_i8] = property_drawers::draw_ro_i8;
    ro_drawers()[(size_t)utils::agea_type::t_i16] = property_drawers::draw_ro_i16;
    ro_drawers()[(size_t)utils::agea_type::t_i32] = property_drawers::draw_ro_i32;
    ro_drawers()[(size_t)utils::agea_type::t_i64] = property_drawers::draw_ro_i64;

    ro_drawers()[(size_t)utils::agea_type::t_u8] = property_drawers::draw_ro_u8;
    ro_drawers()[(size_t)utils::agea_type::t_u16] = property_drawers::draw_ro_u16;
    ro_drawers()[(size_t)utils::agea_type::t_u32] = property_drawers::draw_ro_u32;
    ro_drawers()[(size_t)utils::agea_type::t_u64] = property_drawers::draw_ro_u64;

    ro_drawers()[(size_t)utils::agea_type::t_f] = property_drawers::draw_ro_f;
    ro_drawers()[(size_t)utils::agea_type::t_d] = property_drawers::draw_ro_d;

    ro_drawers()[(size_t)utils::agea_type::t_vec3] = property_drawers::draw_ro_vec3;

    ro_drawers()[(size_t)utils::agea_type::t_mat] = property_drawers::draw_ro_mat;
    ro_drawers()[(size_t)utils::agea_type::t_msh] = property_drawers::draw_ro_msh;

    // clang-format on
}

bool
property_drawers::draw_ro(::agea::model::smart_object* obj, ::agea::reflection::property& p)
{
    size_t idx = (size_t)p.type.type;

    auto ptr = (::agea::blob_ptr)obj;

    ptr = ::agea::reflection::reduce_ptr(ptr + p.offset, p.type.is_ptr);
    ro_drawers()[idx](ptr);

    return false;
}

bool
property_drawers::draw_ro_str(::agea::blob_ptr ptr)
{
    auto& p = agea::reflection::extract<std::string>(ptr);

    ImGui::Text("%s", p.c_str());

    return true;
}

bool
property_drawers::draw_ro_bool(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<bool>(ptr);
    ImGui::Text("%s", (p ? "true" : "false"));

    return true;
}

bool
property_drawers::draw_ro_i8(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::int8_t>(ptr);
    ImGui::Text("%" PRIi8 "", p);

    return true;
}

bool
property_drawers::draw_ro_i16(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::int16_t>(ptr);
    ImGui::Text("%" PRIi16 "", p);

    return true;
}

bool
property_drawers::draw_ro_i32(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::int32_t>(ptr);
    ImGui::Text("%" PRIi32 "", p);

    return true;
}

bool
property_drawers::draw_ro_i64(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::int64_t>(ptr);
    ImGui::Text("%" PRIi64 "", p);

    return true;
}

bool
property_drawers::draw_ro_u8(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::uint8_t>(ptr);
    ImGui::Text("%" PRIu8 "", p);

    return true;
}

bool
property_drawers::draw_ro_u16(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::uint16_t>(ptr);
    ImGui::Text("%" PRIu16 "", p);

    return true;
}

bool
property_drawers::draw_ro_u32(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::uint32_t>(ptr);
    ImGui::Text("%" PRIu32 "", p);

    return true;
}

bool
property_drawers::draw_ro_u64(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<std::uint64_t>(ptr);
    ImGui::Text("%" PRIu64 "", p);

    return true;
}

bool
property_drawers::draw_ro_f(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<float>(ptr);
    ImGui::Text("%f", p);

    return true;
}

bool
property_drawers::draw_ro_d(::agea::blob_ptr ptr)
{
    auto p = agea::reflection::extract<double>(ptr);
    ImGui::Text("%lf", p);

    return true;
}

bool
property_drawers::draw_ro_vec3(::agea::blob_ptr ptr)
{
    auto& p = agea::reflection::extract<glm::vec3>(ptr);
    ImGui::Text("%f %f %f", (p)[0], (p)[1], (p)[2]);

    return true;
}

bool
property_drawers::draw_ro_mat(::agea::blob_ptr ptr)
{
    auto& t = agea::reflection::extract<std::shared_ptr<model::material>>(ptr);
    ImGui::Text("%s", t->get_id().cstr());

    return true;
}

bool
property_drawers::draw_ro_msh(::agea::blob_ptr ptr)
{
    auto& t = agea::reflection::extract<std::shared_ptr<model::mesh>>(ptr);
    ImGui::Text("%s", t->get_id().cstr());

    return true;
}
}  // namespace ui
}  // namespace agea
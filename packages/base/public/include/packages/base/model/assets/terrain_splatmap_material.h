#pragma once

#include "packages/base/model/terrain_splatmap_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
#include "packages/root/model/core_types/vec3.h"
#include "packages/root/model/core_types/vec4.h"

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Terrain material — blends 4 albedo layers by the RGBA channels of a splatmap "
               "with independent per-layer UV tiling. Lit by the standard directional + clustered "
               "lighting path"
);
class terrain_splatmap_material : public ::kryga::root::material
// clang-format on
{
    KRG_gen_meta__terrain_splatmap_material();

public:
    KRG_gen_class_meta(terrain_splatmap_material, ::kryga::root::material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    };

protected:
    // clang-format off
    KRG_ar_property(
        category         = "Properties",
        serializable     = true,
        invalidates      = render,
        access           = all,
        gpu_texture_slot = 0,
        instantiate      = share,
        mcp_hint         = "splatmap weight map — RGBA channels select layers 0..3"
    );
    ::kryga::root::texture_slot m_splatmap;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category         = "Properties",
        serializable     = true,
        invalidates      = render,
        access           = all,
        gpu_texture_slot = 1,
        instantiate      = share,
        mcp_hint         = "albedo layer 0 — selected by splatmap red channel"
    );
    ::kryga::root::texture_slot m_layer0_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category         = "Properties",
        serializable     = true,
        invalidates      = render,
        access           = all,
        gpu_texture_slot = 2,
        instantiate      = share,
        mcp_hint         = "albedo layer 1 — selected by splatmap green channel"
    );
    ::kryga::root::texture_slot m_layer1_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category         = "Properties",
        serializable     = true,
        invalidates      = render,
        access           = all,
        gpu_texture_slot = 3,
        instantiate      = share,
        mcp_hint         = "albedo layer 2 — selected by splatmap blue channel"
    );
    ::kryga::root::texture_slot m_layer2_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category         = "Properties",
        serializable     = true,
        invalidates      = render,
        access           = all,
        gpu_texture_slot = 4,
        instantiate      = share,
        mcp_hint         = "albedo layer 3 — selected by splatmap alpha channel"
    );
    ::kryga::root::texture_slot m_layer3_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        invalidates  = render,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "per-layer UV tiling [x=layer0 y=layer1 z=layer2 w=layer3] — higher = "
                       "more repeats across the terrain"
    );
    ::kryga::root::vec4 m_layer_tiling = {16.0f, 16.0f, 16.0f, 16.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        invalidates  = render,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "ambient color RGB [0-1]"
    );
    ::kryga::root::vec3 m_ambient = {0.15f, 0.15f, 0.15f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "specular highlight color RGB [0-1] — terrain is usually fairly matte"
    );
    ::kryga::root::vec3 m_specular = {0.04f, 0.04f, 0.04f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "shininess exponent — higher = tighter highlights [1-256]"
    );
    float m_shininess = 8.0f;
    // clang-format on
};

}  // namespace base
}  // namespace kryga

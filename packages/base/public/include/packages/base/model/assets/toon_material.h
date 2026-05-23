#pragma once

#include "packages/base/model/toon_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Cel-shaded material — discrete shading bands with configurable band count and "
               "specular strength"
);
class toon_material : public ::kryga::root::material
// clang-format on
{
    KRG_gen_meta__toon_material();

public:
    KRG_gen_class_meta(toon_material, material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    }

    const ::kryga::root::texture_slot&
    diffuse_txt() const
    {
        return m_diffuse_txt;
    }

protected:
    // clang-format off
    KRG_ar_property(
        category         = "Properties",
        serializable     = true,
        invalidates      = render,
        access           = all,
        gpu_texture_slot = 0,
        instantiate      = share,
        mcp_hint         = "diffuse texture slot — set texture ID to change the toon color map"
    );
    ::kryga::root::texture_slot m_diffuse_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        invalidates  = render,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "number of discrete shading bands — 2-8 typical for toon look"
    );
    float m_band_count = 4.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        invalidates  = render,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "specular highlight intensity multiplier [0-1]"
    );
    float m_specular_strength = 1.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        invalidates  = render,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "shininess exponent — higher = tighter highlights [1-256]"
    );
    float m_shininess = 32.0f;
    // clang-format on
};

}  // namespace base
}  // namespace kryga

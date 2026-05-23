#pragma once

#include "packages/base/model/pbr_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
#include "packages/root/model/core_types/vec3.h"

namespace kryga
{
namespace base
{
// clang-format off
KRG_ar_class(
    mcp_hint = "Physically-based material — diffuse/specular textures and ambient/diffuse/specular "
               "color multipliers"
);
class pbr_material : public ::kryga::root::material
// clang-format on
{
    KRG_gen_meta__pbr_material();

public:
    KRG_gen_class_meta(pbr_material, ::kryga::root::material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    };

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "invalidates=render",
        "access=all",
        "gpu_texture_slot=0",
        "instantiate=share",
        "mcp_hint=diffuse/albedo texture slot — set texture ID to change surface color map"
    );
    ::kryga::root::texture_slot m_diffuse_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "invalidates=render",
        "access=all",
        "gpu_texture_slot=1",
        "instantiate=share",
        "mcp_hint=specular/gloss texture slot — controls shininess map"
    );
    ::kryga::root::texture_slot m_specular_txt;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "invalidates=render",
        "check=not_same",
        "access=all",
        "gpu_data=MaterialData",
        "default=true",
        "mcp_hint=ambient color multiplier RGB [0-1]"
    );
    ::kryga::root::vec3 m_ambient = {.2f, .2f, .2f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "access=all",
        "gpu_data=MaterialData",
        "default=true",
        "mcp_hint=diffuse color multiplier RGB [0-1] — multiplied with diffuse texture"
    );
    ::kryga::root::vec3 m_diffuse = {.2f, .2f, .2f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "access=all",
        "gpu_data=MaterialData",
        "default=true",
        "mcp_hint=specular color RGB [0-1] — controls highlight intensity"
    );
    ::kryga::root::vec3 m_specular = 0.5f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "access=no",
        "default=true",
        "gpu_data=MaterialData"
    );
    float m_shininess = 64.0f;
    // clang-format on
};

}  // namespace base
}  // namespace kryga

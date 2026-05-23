#pragma once

#include "packages/base/model/solid_color_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
#include "packages/root/model/core_types/vec3.h"

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Flat-colored material — ambient/diffuse/specular colors and shininess without "
               "textures"
);
class solid_color_material : public ::kryga::root::material
// clang-format on
{
    KRG_gen_meta__solid_color_material();

public:
    KRG_gen_class_meta(solid_color_material, material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

protected:
    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "invalidates=render",
        "check=not_same",
        "access=all",
        "gpu_data=MaterialData",
        "default=true",
        "mcp_hint=ambient color RGB [0-1]"
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
        "mcp_hint=base surface color RGB [0-1]"
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
        "mcp_hint=specular highlight color RGB [0-1]"
    );
    ::kryga::root::vec3 m_specular = 0.5f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "access=all",
        "gpu_data=MaterialData",
        "default=true",
        "mcp_hint=shininess exponent — higher = tighter highlights [1-256]"
    );
    float m_shininess = 32.0f;
    // clang-format on
};

}  // namespace base
}  // namespace kryga

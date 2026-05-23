#pragma once

#include "packages/base/model/simple_texture_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
#include "packages/root/model/core_types/vec3.h"

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Single-texture material — maps one texture onto the surface with basic lighting"
);
class simple_texture_material : public ::kryga::root::material
// clang-format on
{
    KRG_gen_meta__simple_texture_material();

public:
    KRG_gen_class_meta(simple_texture_material, material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

    const ::kryga::root::texture_slot&
    simple_texture() const
    {
        return m_simple_texture;
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
        mcp_hint         = "the single texture applied to the surface — set texture ID to change"
    );
    ::kryga::root::texture_slot m_simple_texture;
    // clang-format on

    // GPU data properties — needed for argen to generate simple_texture_material__gpu
    // with texture_indices/sampler_indices arrays. Values are unused for texture-only materials.
    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = false,
        gpu_data     = MaterialData,
        default      = true
    );
    ::kryga::root::vec3 m_ambient = {0.2f, 0.2f, 0.2f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = false,
        gpu_data     = MaterialData,
        default      = true
    );
    ::kryga::root::vec3 m_diffuse = {1.0f, 1.0f, 1.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = false,
        gpu_data     = MaterialData,
        default      = true
    );
    ::kryga::root::vec3 m_specular = {0.0f, 0.0f, 0.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = false,
        gpu_data     = MaterialData,
        default      = true
    );
    float m_shininess = 1.0f;
    // clang-format on
};

}  // namespace base
}  // namespace kryga

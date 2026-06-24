#pragma once

#include "packages/root/model/solid_color_alpha_material.ar.h"

#include "packages/root/model/assets/solid_color_material.h"

namespace kryga
{
namespace root
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Transparent solid color material — inherits solid_color_material colors and adds "
               "opacity control"
);
class solid_color_alpha_material : public solid_color_material
// clang-format on
{
    KRG_gen_meta__solid_color_alpha_material();

public:
    KRG_gen_class_meta(solid_color_alpha_material, solid_color_material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = all,
        gpu_data     = MaterialData,
        default      = true,
        mcp_hint     = "transparency 0.0=fully transparent 1.0=fully opaque"
    );
    float m_opacity = 1.0f;
    // clang-format on
};

}  // namespace root
}  // namespace kryga

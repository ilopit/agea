#pragma once

#include "packages/base/model/solid_color_alpha_material.ar.h"

#include "packages/base/model/assets/solid_color_material.h"

namespace kryga
{
namespace base
{

KRG_ar_class();
class solid_color_alpha_material : public solid_color_material
{
    KRG_gen_meta__solid_color_alpha_material();

public:
    KRG_gen_class_meta(solid_color_alpha_material, solid_color_material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

protected:
    KRG_ar_property("category=Properties",
                    "serializable=true",
                    "access=all",
                    "gpu_data=MaterialData",
                    "default=true");
    float m_opacity = 1.0f;
};

}  // namespace base
}  // namespace kryga

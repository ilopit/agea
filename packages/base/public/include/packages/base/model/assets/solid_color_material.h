#pragma once

#include "packages/base/model/solid_color_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_sample.h"
#include "packages/root/model/core_types/vec3.h"

namespace agea
{
namespace base
{

AGEA_ar_class();
class solid_color_material : public ::agea::root::material
{
    AGEA_gen_meta__solid_color_material();

public:
    AGEA_gen_class_meta(solid_color_material, material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

protected:
    AGEA_ar_property("category=Properties",
                     "serializable=true",
                     "invalidates=render",
                     "check=not_same",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::agea::root::vec3 m_ambient = {.2f, .2f, .2f};

    AGEA_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::agea::root::vec3 m_diffuse = {.2f, .2f, .2f};

    AGEA_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::agea::root::vec3 m_specular = 0.5f;

    AGEA_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    float m_shininess = 32.0f;
};

}  // namespace base
}  // namespace agea

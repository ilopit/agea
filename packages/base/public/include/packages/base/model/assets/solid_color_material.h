#pragma once

#include "packages/base/model/solid_color_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_sample.h"
#include "packages/root/model/core_types/vec3.h"

namespace kryga
{
namespace base
{

KRG_ar_class();
class solid_color_material : public ::kryga::root::material
{
    KRG_gen_meta__solid_color_material();

public:
    KRG_gen_class_meta(solid_color_material, material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

protected:
    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "invalidates=render",
                     "check=not_same",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::kryga::root::vec3 m_ambient = {.2f, .2f, .2f};

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::kryga::root::vec3 m_diffuse = {.2f, .2f, .2f};

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::kryga::root::vec3 m_specular = 0.5f;

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    float m_shininess = 32.0f;
};

}  // namespace base
}  // namespace kryga

#pragma once

#include "packages/base/model/pbr_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/core_types/vec3.h"

namespace agea
{
namespace base
{
AGEA_ar_class();
class pbr_material : public ::agea::root::material
{
    AGEA_gen_meta__pbr_material();

public:
    AGEA_gen_class_meta(pbr_material, ::agea::root::material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    };

    AGEA_ar_property(
        "category=Properties",
        "serializable=true",
        "property_des_handler=::agea::root::property_texture_sample__deserialize",
        "property_ser_handler=::agea::root::property_texture_sample__serialize",
        "property_compare_handler=::agea::root::property_texture_sample__compare",
        "property_instantiate_handler=::agea::root::property_texture_sample__instantiate",
        "property_copy_handler=::agea::root::property_texture_sample__copy");
    ::agea::root::texture_sample m_diffuse_txt;

    AGEA_ar_property(
        "category=Properties",
        "serializable=true",
        "property_des_handler=::agea::root::property_texture_sample__deserialize",
        "property_ser_handler=::agea::root::property_texture_sample__serialize",
        "property_compare_handler=::agea::root::property_texture_sample__compare",
        "property_instantiate_handler=::agea::root::property_texture_sample__instantiate",
        "property_copy_handler=::agea::root::property_texture_sample__copy");
    ::agea::root::texture_sample m_specular_txt;

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
                     "access=no",
                     "default=true",
                     "gpu_data=MaterialData");
    float m_shininess = 64.0f;
};

}  // namespace base
}  // namespace agea

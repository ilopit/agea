#pragma once

#include "root/pbr_material.generated.h"

#include "root/assets/material.h"

namespace agea
{
namespace root
{
AGEA_ar_class();
class pbr_material : public material
{
    AGEA_gen_meta__pbr_material();

public:
    AGEA_gen_class_meta(pbr_material, material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    };

    AGEA_ar_property("category=meta",
                     "serializable=true",
                     "property_des_handler=custom::texture_sample_deserialize",
                     "property_ser_handler=custom::texture_sample_serialize",
                     "property_prototype_handler=custom::texture_sample_prototype",
                     "property_compare_handler=custom::texture_sample_compare",
                     "property_copy_handler=custom::texture_sample_copy");
    texture_sample m_diffuse_txt;

    AGEA_ar_property("category=meta",
                     "serializable=true",
                     "property_des_handler=custom::texture_sample_deserialize",
                     "property_ser_handler=custom::texture_sample_serialize",
                     "property_prototype_handler=custom::texture_sample_prototype",
                     "property_compare_handler=custom::texture_sample_compare",
                     "property_copy_handler=custom::texture_sample_copy");
    texture_sample m_specular_txt;

    AGEA_ar_property("category=properties",
                     "serializable=true",
                     "invalidates=render",
                     "check=not_same",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    vec3 m_ambient = {.2f, .2f, .2f};

    AGEA_ar_property("category=properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    vec3 m_diffuse = {.2f, .2f, .2f};

    AGEA_ar_property("category=properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    vec3 m_specular = 0.5f;

    AGEA_ar_property(
        "category=meta", "serializable=true", "access=no", "default=true", "gpu_data=MaterialData");
    float m_shininess = 64.0f;
};

}  // namespace root
}  // namespace agea

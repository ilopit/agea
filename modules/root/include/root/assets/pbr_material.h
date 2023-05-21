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
    texture_sample m_diffuse;

    AGEA_ar_property("category=meta",
                     "serializable=true",
                     "property_des_handler=custom::texture_sample_deserialize",
                     "property_ser_handler=custom::texture_sample_serialize",
                     "property_prototype_handler=custom::texture_sample_prototype",
                     "property_compare_handler=custom::texture_sample_compare",
                     "property_copy_handler=custom::texture_sample_copy");
    texture_sample m_specular;

    AGEA_ar_property(
        "category=meta", "serializable=true", "access=no", "default=true", "gpu_data=MaterialData");
    float m_shininess = 64.0f;
};

}  // namespace root
}  // namespace agea
